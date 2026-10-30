"""Relocate the build host's CPython/Tk and pinned Online wheels into a Mac package."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import ssl
import subprocess
import sys
import sysconfig
import zipfile

from bundle_online import COMPANION, repo_file


def run(*args):
    return subprocess.check_output(list(map(str, args)), text=True).strip()


def dependencies(path):
    identities = set(run('otool', '-D', path).splitlines()[1:])
    return [line.strip().split(' (', 1)[0] for line in run('otool', '-L', path).splitlines()[1:]
            if line.startswith((' ', '\t')) and line.strip().split(' (', 1)[0] not in identities]


def system_library(path):
    return path.startswith(('/usr/lib/', '/System/Library/'))


def relocate(pairs, runtime):
    """Rewrite every extension and the complete dylib closure, then ad-hoc sign."""
    library = runtime/'lib/native'
    library.mkdir(parents=True)
    pending = list(pairs)
    visited, origins, records = set(), {}, {}
    while pending:
        source, target = pending.pop(0)
        if target in visited:
            continue
        visited.add(target)
        if 'arm64' not in run('lipo', '-archs', source).split():
            raise ValueError('Online dependency lacks arm64: '+str(source))
        target.chmod(target.stat().st_mode | 0o200)
        for dependency in dependencies(source):
            if system_library(dependency):
                continue
            name = Path(dependency).name
            resolved = Path(dependency)
            if dependency.startswith('@'):
                # Extensions supplied by wheels use adjacent libraries. Reject
                # unresolvable rpaths instead of accidentally using the host.
                resolved = source.parent/name
            if not resolved.is_file():
                raise ValueError('Unresolved Online dependency: '+dependency)
            resolved = resolved.resolve()
            destination = library/name
            if name in origins and origins[name] != resolved:
                raise ValueError('Conflicting Online library: '+name)
            if name not in origins:
                origins[name] = resolved
                shutil.copy2(resolved, destination)
                records[name] = dict(source=str(resolved), sha256=hashlib.sha256(resolved.read_bytes()).hexdigest())
                pending.append((resolved, destination))
            reference = '@loader_path/'+os.path.relpath(destination, target.parent)
            subprocess.run(['install_name_tool', '-change', dependency, reference, str(target)], check=True, capture_output=True)
        if target.parent == library:
            subprocess.run(['install_name_tool', '-id', '@loader_path/'+target.name, str(target)], check=True, capture_output=True)
    for target in visited:
        for dependency in dependencies(target):
            if system_library(dependency):
                continue
            if not dependency.startswith('@loader_path/'):
                raise ValueError('External Online dependency remains: '+dependency)
            resolved = (target.parent/dependency.removeprefix('@loader_path/')).resolve()
            if not resolved.is_relative_to(runtime.resolve()) or not resolved.is_file():
                raise ValueError('Missing relocated Online dependency: '+dependency)
        subprocess.run(['codesign', '--force', '--sign', '-', '--timestamp=none', str(target)], check=True, capture_output=True)
        subprocess.run(['codesign', '--verify', '--strict', str(target)], check=True, capture_output=True)
    return records


def copy_notices(formula, copy):
    """Keep formula notice bytes under the installer's flat .txt license layout."""
    for notice in sorted(formula.iterdir()):
        if notice.is_file() and (notice.name.lower().startswith(('license', 'copying', 'copyright'))
                                or notice.name == 'sbom.spdx.json'):
            name = 'Online-'+formula.parent.name+'-'+notice.name
            if not name.lower().endswith('.txt'):
                name += '.txt'
            copy(notice, 'Licenses/'+name)


def bundle(root, stage, copy, generated=None):
    if sys.platform != 'darwin':
        raise RuntimeError('Bundle Online on the native Mac build host.')
    import tkinter
    import _tkinter
    generated = generated or root/'build/macos/generated'
    runtime = stage/'Support/Python'
    version = f'{sys.version_info.major}.{sys.version_info.minor}'
    base = Path(sys.base_prefix)
    standard = Path(sysconfig.get_path('stdlib'))
    library = runtime/f'lib/python{version}'
    # Never copy a developer venv or unrelated installed packages.
    shutil.copytree(standard, library, ignore=shutil.ignore_patterns(
        'site-packages', '__pycache__', '*.pyc', 'test', 'tests', 'ensurepip', 'idlelib', 'turtledemo'))
    # Framework bin/python is a launcher that respawns Resources/Python.app.
    # Bundle the actual interpreter so the game remains its direct parent.
    framework_python = base/'Resources/Python.app/Contents/MacOS/Python'
    executable = (framework_python if framework_python.is_file() else base/f'bin/python{version}').resolve()
    copy(executable, 'Support/Python/bin/python-real')
    tkinter_target = library/'lib-dynload'/Path(_tkinter.__file__).name
    shutil.copy2(_tkinter.__file__, tkinter_target)
    interpreter = tkinter.Tcl()
    tcl = Path(interpreter.eval('info library'))
    window = tkinter.Tk(); window.withdraw()
    try:
        tk = Path(window.tk.eval('set tk_library'))
    finally:
        window.destroy()
    for source, name in ((tcl, 'tcl'), (tk, 'tk')):
        shutil.copytree(source, runtime/'lib'/name, ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
    certificate = ssl.get_default_verify_paths().cafile
    if not certificate:
        raise RuntimeError('Build host has no CA bundle for the portable HTTPS client.')
    copy(Path(certificate), 'Support/Python/certificates.pem')
    wheels = root/'build/release-wheels-macos'
    wheels.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, '-m', 'pip', '--disable-pip-version-check', 'download',
                    '--only-binary=:all:', '--dest', str(wheels), '-r', str(root/'requirements-release.txt')], check=True)
    wheel_records = []
    for requirement in (root/'requirements-release.txt').read_text().splitlines():
        if not requirement or requirement.startswith('#'):
            continue
        name, version_pin = requirement.split('==')
        candidates = list(wheels.glob(f'{name.replace("-", "_")}-{version_pin}-*.whl'))
        if len(candidates) != 1:
            raise ValueError('Expected exactly one pinned Mac wheel for '+requirement)
        wheel = candidates[0]
        with zipfile.ZipFile(wheel) as archive:
            for item in archive.namelist():
                if not (library/'site-packages'/item).resolve().is_relative_to((library/'site-packages').resolve()):
                    raise ValueError('Invalid wheel path')
            archive.extractall(library/'site-packages')
        wheel_records.append(dict(file=wheel.name, sha256=hashlib.sha256(wheel.read_bytes()).hexdigest()))
    pairs = [(executable, runtime/'bin/python-real')]
    for target in library.rglob('*.so'):
        relative = target.relative_to(library)
        source = standard/relative
        if target == tkinter_target:
            source = Path(_tkinter.__file__)
        elif relative.parts[0] == 'site-packages':
            source = target
        pairs.append((source.resolve(), target))
    records = relocate(pairs, runtime)
    wrapper = runtime/'bin/python3'
    wrapper.write_text('#!/bin/sh\nset -eu\n'
        'ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)\n'
        'export PYTHONHOME="$ROOT" PYTHONNOUSERSITE=1 PYTHONDONTWRITEBYTECODE=1\n'
        'export TCL_LIBRARY="$ROOT/lib/tcl" TK_LIBRARY="$ROOT/lib/tk"\n'
        'export SSL_CERT_FILE="$ROOT/certificates.pem"\n'
        'unset PYTHONPATH PYTHONUSERBASE __PYVENV_LAUNCHER__\n'
        'unset DYLD_LIBRARY_PATH DYLD_FRAMEWORK_PATH DYLD_FALLBACK_LIBRARY_PATH\n'
        'exec "$ROOT/bin/python-real" "$@"\n', encoding='utf-8')
    wrapper.chmod(0o755)
    for name in COMPANION:
        copy(repo_file(root, 'tools', name), 'Support/Online/'+name)
    copy(root/'build/online-icon/multiplayer.png', 'Support/Online/multiplayer.png')
    manifest = json.loads((generated/'network-identity.json').read_text())
    manifest['files'] = [entry for entry in manifest['files'] if entry[0].startswith('game/')]
    (stage/'Support/Online/network-identity.json').write_text(json.dumps(manifest, indent=2)+'\n')
    copy(generated/'network-build.json', 'Support/Online/network-build.json')
    # Preserve notices from each installed formula whose libraries we relocate.
    formulae = set()
    for source in [executable, tcl.resolve(), tk.resolve(), *(Path(r['source']) for r in records.values())]:
        parts = source.parts
        if 'Cellar' in parts:
            index = parts.index('Cellar')
            formulae.add(Path(*parts[:index+3]))
    for formula in sorted(formulae):
        copy_notices(formula, copy)
    (stage/'Licenses/Online-dependencies.txt').write_text(
        'CPython, Tcl/Tk and native dependencies: notices and package source metadata in Licenses/Online-*.txt.\n'
        'Pinned Python wheel notices: Support/Python/lib/python'+version+'/site-packages/*dist-info/licenses.\n'
        'Public CA trust store: Support/Python/certificates.pem.\n', encoding='utf-8')
    probe = stage/'Support/Online/bundle-check.py'
    probe.write_text('import ssl, tkinter, tempfile\nfrom pathlib import Path\n'
        'from online_host import certificate\nfrom online_theme import OnlineTheme\n'
        'window=tkinter.Tk(); window.withdraw(); theme=OnlineTheme(window); window.update(); window.destroy()\n'
        'with tempfile.TemporaryDirectory() as temp:\n'
        ' cert,key,pin=certificate(Path(temp)/"directory")\n'
        ' context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); context.load_cert_chain(cert,key)\n'
        'assert ssl.create_default_context().cert_store_stats()["x509_ca"] > 0\n'
        'print("PASS: relocated Mac Python, Tk, TLS, certificates and multiplayer theme")\n', encoding='utf-8')
    try:
        subprocess.run([str(wrapper), '-B', str(probe)], check=True, timeout=30)
    finally:
        probe.unlink()
    return dict(python=sys.version.split()[0], wheels=wheel_records, native_libraries=records,
                compatibility=manifest['compatibility'])
