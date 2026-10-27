"""Bundle an isolated Windows Python/Tk runtime and the Online companion."""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import zipfile

COMPANION = ('online.py', 'online_client.py', 'online_transport.py', 'online_host.py',
             'online_paths.py', 'online_theme.py', 'lobby_server.py', 'network-identity.py')


def bundle_standard_library(base, runtime, version):
    """Preserve SSL's adjacent trust resources when relocating CPython."""
    library_root = base/'Lib'
    with zipfile.ZipFile(runtime/f'python{version}.zip', 'w', zipfile.ZIP_DEFLATED) as library:
        for path in sorted(library_root.rglob('*.py')):
            relative = path.relative_to(library_root)
            if relative.as_posix() != 'ssl.py' and not set(relative.parts) & {
                    'site-packages', '__pycache__', 'test', 'tests',
                    'ensurepip', 'idlelib', 'turtledemo', 'venv'}:
                library.write(path, relative.as_posix())
    # SSL may load a distribution's trust policy relative to __file__. OpenSSL
    # needs a real filesystem path, so neither the module nor its PEM resources
    # can live only inside pythonXY.zip. Preserve policy bytes without edits.
    shutil.copy2(library_root/'ssl.py', runtime/'ssl.py')
    for path in sorted(library_root.glob('*.pem')):
        shutil.copy2(path, runtime/path.name)


def bundle(root, stage, copy):
    base = Path(sys.base_prefix)
    if sys.platform != 'win32' or sys.maxsize <= 2**32 or not (base/'tcl').is_dir():
        raise RuntimeError('Package with x64 Windows CPython including Tcl/Tk.')
    version = f'{sys.version_info.major}{sys.version_info.minor}'
    runtime = stage/'Support/Python'
    for name in ('python.exe', 'pythonw.exe', f'python{version}.dll', 'python3.dll',
                 'vcruntime140.dll', 'vcruntime140_1.dll', 'LICENSE.txt'):
        copy(base/name, 'Support/Python/'+name)
    for directory in ('DLLs', 'tcl'):
        for path in sorted((base/directory).rglob('*')):
            if path.is_file() and path.suffix.lower() not in ('.pdb', '.pyc'):
                copy(path, 'Support/Python/'+path.relative_to(base).as_posix())
    # Do not copy the developer's environment, pip, tests, or cached bytecode.
    bundle_standard_library(base, runtime, version)
    # _pth disables registry/PYTHONPATH/site customizations, making relocation
    # independent of Python installations on the tester's machine.
    (runtime/f'python{version}._pth').write_text(
        f'python{version}.zip\n.\nDLLs\nsite-packages\n../Online\n', encoding='ascii')
    wheel_dir = root/'build/release-wheels'
    wheel_dir.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, '-m', 'pip', '--disable-pip-version-check', 'download', '--only-binary=:all:',
                    '--dest', str(wheel_dir), '-r', str(root/'requirements-release.txt')], check=True)
    wheels = []
    requirements = [line for line in (root/'requirements-release.txt').read_text().splitlines()
                    if line and not line.startswith('#')]
    for requirement in requirements:
        name, release = requirement.split('==')
        matches = list(wheel_dir.glob(f'{name.replace("-", "_")}-{release}-*.whl'))
        if len(matches) != 1:
            raise RuntimeError(f'Expected one matching wheel for {requirement}; found {matches}')
        wheel = matches[0]
        with zipfile.ZipFile(wheel) as archive:
            for member in archive.infolist():
                target = runtime/'site-packages'/member.filename
                if not target.resolve().is_relative_to((runtime/'site-packages').resolve()):
                    raise RuntimeError('Invalid dependency wheel path')
            archive.extractall(runtime/'site-packages')
        wheels.append(dict(file=wheel.name, sha256=hashlib.sha256(wheel.read_bytes()).hexdigest()))
    for name in COMPANION:
        copy(repo_file(root, 'tools', name), 'Support/Online/'+name)
    manifest = json.loads((root/'build/windows/generated/network-identity.json').read_text())
    manifest['files'] = [entry for entry in manifest['files'] if entry[0].startswith('game/')]
    (stage/'Support/Online/network-identity.json').write_text(json.dumps(manifest, indent=2)+'\n')
    copy(root/'build/windows/generated/network-build.json', 'Support/Online/network-build.json')
    copy(base/'LICENSE.txt', 'Licenses/Python.txt')
    (stage/'Licenses/Online-dependencies.txt').write_text(
        'CPython and Tcl/Tk: Support/Python/LICENSE.txt (includes third-party notices).\n'
        'cryptography, cffi, pycparser and bundled OpenSSL notices: the license files\n'
        'in Support/Python/site-packages/*dist-info/licenses (also included in this package).\n'
        'Python source: https://www.python.org/downloads/source/\n'
        'cryptography source: https://github.com/pyca/cryptography\n'
        'cffi source: https://github.com/python-cffi/cffi\n'
        'pycparser source: https://github.com/eliben/pycparser\n', encoding='utf-8')
    # Exercise Tk and native extensions using the bundled interpreter, before
    # advertising the package as independent of a system Python installation.
    probe = stage/'Support/Online/bundle-check.py'
    probe.write_text('import ssl, tkinter, cryptography\n'
                     'from online_paths import ROOT, PORTABLE\n'
                     'from online_host import certificate\n'
                     'import tempfile\nfrom pathlib import Path\n'
                     'assert PORTABLE\n'
                     'window=tkinter.Tk(); window.withdraw(); window.update(); window.destroy()\n'
                     'with tempfile.TemporaryDirectory(prefix="tsfp-cert-check-") as temp:\n'
                     '    cert,key,pin=certificate(Path(temp)/"directory")\n'
                     '    context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)\n'
                     '    context.load_cert_chain(cert,key)\n'
                     'print("PASS: bundled Python, Tk, TLS and certificate generation")\n')
    try:
        subprocess.run([str(runtime/'python.exe'), '-B', str(probe)], check=True)
    finally:
        probe.unlink()
    return dict(python=sys.version.split()[0], wheels=wheels, compatibility=manifest['compatibility'])
