"""Stage a relocatable private Mac workshop runtime; never installs or starts services."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys

CELLAR = Path('/opt/homebrew/Cellar')
MAX_BYTES = 512 * 1024**2
MAX_FILES = 20000
MAGIC = {b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf', b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca', b'\xca\xfe\xba\xbf'}
LOADS = {'LC_LOAD_DYLIB', 'LC_LOAD_WEAK_DYLIB', 'LC_REEXPORT_DYLIB', 'LC_LOAD_UPWARD_DYLIB', 'LC_LAZY_LOAD_DYLIB'}
wheel_spec = importlib.util.spec_from_file_location('runtime_wheels', Path(__file__).with_name('runtime_wheels.py'))
wheels = importlib.util.module_from_spec(wheel_spec)
wheel_spec.loader.exec_module(wheels)


def require(value, message):
    if not value:
        raise ValueError(message)


def run(*args):
    result = subprocess.run(args, stdin=subprocess.DEVNULL, capture_output=True, timeout=30)
    require(len(result.stdout) + len(result.stderr) <= 4 * 1024**2, 'Runtime command output exceeded budget')
    require(result.returncode == 0, 'Runtime command failed: ' + Path(args[0]).name + ': ' + result.stderr.decode('utf-8', 'replace')[:400])
    return result.stdout.decode('utf-8')


def commands(text):
    """Read explicit load commands, excluding a dylib's own install identity."""
    dependencies, rpaths, current, count = [], [], None, 0
    for line in text.splitlines():
        fields = line.strip().split()
        if len(fields) == 2 and fields[0] == 'cmd':
            current = fields[1]
            count += 1
            require(current != 'LC_DYLD_ENVIRONMENT', 'Embedded loader environment refused')
        match = re.fullmatch(r'\s*(name|path) (.+) \(offset [0-9]+\)', line)
        if match:
            kind, value = match.groups()
            require(not any(c in value for c in '\r\n\0'), 'Invalid loader path')
            if current in LOADS and kind == 'name':
                dependencies.append(value)
            elif current == 'LC_RPATH' and kind == 'path':
                rpaths.append(value)
    require(count > 0, 'Missing native load commands')
    return dependencies, rpaths


def system_path(value):
    return value.startswith(('/usr/lib/', '/System/Library/')) and '..' not in Path(value).parts


def source(path):
    path = Path(path).resolve(strict=True)
    require(path.is_relative_to(CELLAR) and len(path.relative_to(CELLAR).parts) >= 3, 'Runtime input is outside Homebrew Cellar')
    info = path.stat()
    require(stat.S_ISREG(info.st_mode) and info.st_uid in (0, os.getuid()) and not info.st_mode & 0o6022,
            'Unsafe runtime source file')
    return path


def resolve_dependency(name, image, rpaths, approved=()):
    if system_path(name):
        return None
    if name.startswith('@loader_path/'):
        candidates = [image.parent / name[len('@loader_path/'):]]
    elif name.startswith('@rpath/'):
        candidates = []
        for path in rpaths:
            if path.startswith('@loader_path/'):
                path = str(image.parent / path[len('@loader_path/'):])
            require(path.startswith('/'), 'Unsupported relative runtime search path')
            candidates.append(Path(path) / name[len('@rpath/'):])
    else:
        require(name.startswith('/'), 'Unsupported runtime dependency: ' + name)
        candidates = [Path(name)]
    found = set()
    for candidate in candidates:
        if candidate.exists():
            resolved = candidate.resolve(strict=True)
            found.add(resolved if resolved in approved else source(resolved))
    require(len(found) == 1, 'Missing or ambiguous runtime dependency: ' + name)
    return found.pop()


def license_files(formula):
    notices = [p for p in formula.iterdir() if p.is_file() and p.name.lower().startswith(('license', 'copying', 'copyright', 'notice'))]
    if not notices and formula.parent.name == 'sqlite':
        header = formula / 'include/sqlite3.h'
        require(header.is_file() and b'The author disclaims copyright to this source code.' in header.read_bytes()[:2048],
                'SQLite header license notice is missing')
        notices = [header]
    require(notices, 'No license found for runtime dependency: ' + str(formula))
    return notices


def sign_images(output, images, runner=run):
    # Framework/app seals include nested code. Finish every rewrite, sign the
    # deepest nested images first, then verify the entire finished set.
    paths = sorted(set(images), key=lambda p: (-len(p.parts), str(p)))
    for relative in paths:
        runner('/usr/bin/codesign', '--force', '--sign', '-', str(output / relative))
    for relative in paths:
        runner('/usr/bin/codesign', '--verify', '--strict', str(output / relative))


class Builder:
    def __init__(self, output):
        self.output, self.total, self.records, self.images = output, 0, {}, {}

    def copy(self, original, relative):
        original = source(original)
        require(not relative.is_absolute() and '..' not in relative.parts, 'Escaped runtime destination')
        require(str(relative) not in self.records, 'Duplicate runtime destination')
        info = original.stat()
        require(len(self.records) < MAX_FILES and self.total + info.st_size <= MAX_BYTES, 'Runtime staging budget exceeded')
        target = self.output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        digest = hashlib.sha256()
        fd = os.open(original, os.O_RDONLY | os.O_NOFOLLOW)
        with os.fdopen(fd, 'rb') as incoming, target.open('xb') as outgoing:
            before = os.fstat(incoming.fileno())
            count = 0
            while wire := incoming.read(1024 * 1024):
                count += len(wire)
                require(count <= info.st_size, 'Runtime source grew during staging')
                outgoing.write(wire)
                digest.update(wire)
            after = os.fstat(incoming.fileno())
        require(count == info.st_size and (before.st_ino, before.st_size, before.st_mtime_ns)
                == (after.st_ino, after.st_size, after.st_mtime_ns), 'Runtime source changed during staging')
        target.chmod(0o755 if info.st_mode & 0o111 else 0o644)
        self.total += count
        self.records[str(relative)] = dict(source=str(original), source_sha256=digest.hexdigest(), bytes=count)
        with target.open('rb') as stream:
            if stream.read(4) in MAGIC:
                require(original not in self.images, 'Duplicate native image source')
                self.images[original] = relative
        return target

    def python(self, executable):
        executable = source(executable)
        version = executable.parent.parent
        require(version.parent.parent.name == 'Python.framework' and version.parent.name == 'Versions',
                'Expected a Homebrew Python framework executable')
        destination = Path('Frameworks/Python.framework/Versions') / version.name
        pending = [version]
        links = []
        while pending:
            directory = pending.pop()
            for child in sorted(directory.iterdir()):
                relative = child.relative_to(version)
                if child.name in ('__pycache__', 'site-packages', '_CodeSignature'):
                    continue
                require(len(self.records) + len(pending) + len(links) < MAX_FILES, 'Python tree exceeds budget')
                if child.is_symlink():
                    resolved = child.resolve(strict=True)
                    require(resolved.is_relative_to(version), 'External Python framework link')
                    links.append((destination / relative, destination / resolved.relative_to(version)))
                elif child.is_dir():
                    pending.append(child)
                else:
                    self.copy(child, destination / relative)
        for relative, target in links:
            path = self.output / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            link = os.path.relpath(self.output / target, path.parent)
            path.symlink_to(link)
            self.records[str(relative)] = dict(symlink=link)
        return destination / executable.relative_to(version)

    def relocate(self):
        inspected = {}
        while len(inspected) < len(self.images):
            for original, relative in list(self.images.items()):
                if original in inspected:
                    continue
                dependencies, rpaths = commands(run('/usr/bin/otool', '-l', str(original)))
                edges = []
                for name in dependencies:
                    resolved = resolve_dependency(name, original, rpaths, self.images)
                    if resolved is not None and resolved not in self.images:
                        key = hashlib.sha256(str(resolved).encode()).hexdigest()[:16]
                        self.copy(resolved, Path('lib') / (key + '-' + resolved.name))
                    edges.append((name, resolved))
                inspected[original] = (edges, rpaths)
        for original, (edges, rpaths) in inspected.items():
            target = self.output / self.images[original]
            args = ['/usr/bin/install_name_tool']
            for name, resolved in edges:
                if resolved is not None:
                    replacement = '@loader_path/' + os.path.relpath(self.output / self.images[resolved], target.parent)
                    args.extend(['-change', name, replacement])
            for path in dict.fromkeys(rpaths):
                args.extend(['-delete_rpath', path])
            # IDs are informational; dependency edges carry explicit loader-relative paths.
            if len(args) > 1:
                run(*args, str(target))
            actual, remaining = commands(run('/usr/bin/otool', '-l', str(target)))
            require(not remaining and all(system_path(n) or n.startswith('@loader_path/') for n in actual),
                    'Unconfined staged runtime dependency')
            for name in actual:
                if not system_path(name):
                    dependency = (target.parent / name[len('@loader_path/'):]).resolve(strict=True)
                    require(dependency.is_relative_to(self.output), 'Staged runtime dependency escaped bundle')
        sign_images(self.output, self.images.values())
        return inspected

    def packages(self, wheelhouse, python):
        requirements = [Path(__file__).with_name(n) for n in ('requirements.txt', 'requirements-signing.txt')]
        pins = wheels.parse_pins([p.read_text(encoding='utf-8') for p in requirements])
        relative = python.parent.parent / 'lib' / ('python' + python.parent.parent.name) / 'site-packages'
        packages, records = wheels.install(wheelhouse, self.output / relative, pins)
        for name, record in records.items():
            path = relative / name
            require(str(path) not in self.records, 'Wheel overwrote runtime payload')
            self.total += record['bytes']
            require(len(self.records) < MAX_FILES and self.total <= MAX_BYTES, 'Combined runtime exceeds budget')
            self.records[str(path)] = record
            target = self.output / path
            with target.open('rb') as stream:
                if stream.read(4) in MAGIC:
                    self.images[target] = path
        return dict(packages=packages,
                    requirements={p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in requirements},
                    producer_sha256=hashlib.sha256(Path(wheels.__file__).read_bytes()).hexdigest())

    def licenses(self):
        formulas = set()
        for record in self.records.values():
            if 'source' in record:
                parts = Path(record['source']).relative_to(CELLAR).parts
                formulas.add(CELLAR / parts[0] / parts[1])
        for formula in sorted(formulas):
            for path in license_files(formula):
                self.copy(path, Path('Licenses') / formula.relative_to(CELLAR) / path.relative_to(formula))

    def audit(self, python):
        framework = python.parent.parent
        generated = {str(framework / '_CodeSignature/CodeResources'),
                     str(framework / 'Resources/Python.app/Contents/_CodeSignature/CodeResources')}
        actual, total = set(), 0
        for path in self.output.rglob('*'):
            relative = str(path.relative_to(self.output))
            info = path.lstat()
            require(info.st_uid == os.getuid() and (stat.S_ISLNK(info.st_mode) or not info.st_mode & 0o022),
                    'Unsafe staged runtime ownership/mode')
            if stat.S_ISDIR(info.st_mode):
                continue
            require(len(actual) < MAX_FILES, 'Runtime audit entry budget exceeded')
            actual.add(relative)
            if relative not in self.records:
                require(relative in generated and stat.S_ISREG(info.st_mode), 'Unrecorded staged runtime file')
                self.records[relative] = dict(generated='codesign')
            record = self.records[relative]
            if 'symlink' in record:
                require(path.is_symlink() and str(path.readlink()) == record['symlink']
                        and path.resolve(strict=True).is_relative_to(self.output), 'Changed staged runtime link')
            else:
                require(stat.S_ISREG(info.st_mode) and info.st_nlink == 1, 'Linked or special staged runtime file')
                total += info.st_size
                require(total <= MAX_BYTES, 'Final runtime exceeds storage budget')
                record.update(sha256=hashlib.sha256(path.read_bytes()).hexdigest(), bytes=info.st_size,
                              mode=stat.S_IMODE(info.st_mode))
        require(actual == set(self.records), 'Staged runtime file disappeared')
        return total


def build(args):
    require(sys.platform == 'darwin' and os.getuid() != 0, 'Stage as the ordinary Mac user, without sudo')
    output = Path(os.path.abspath(args.output))
    require(all(not p.is_symlink() for p in (output, *output.parents)), 'Linked staging directory refused')
    require(output.parent.is_dir() and output.parent.stat().st_uid == os.getuid()
            and not output.parent.stat().st_mode & 0o077, 'Choose a private owner-only staging parent')
    require(shutil.disk_usage(output.parent).free >= 1024**3, 'Need at least 1 GiB free before runtime staging')
    output.mkdir(mode=0o700)
    builder = Builder(output)
    python = builder.python(args.python)
    for name in ('clamscan', 'freshclam', 'caddy'):
        builder.copy(getattr(args, name), Path('bin') / name)
    applications = builder.packages(args.wheelhouse, python) if args.wheelhouse else None
    builder.relocate()
    builder.licenses()
    payload_bytes = builder.audit(python)
    # A genuine isolated invocation must find the staged stdlib, not Homebrew's.
    script = 'import json,sys,ssl,sqlite3,zlib,ctypes;print(json.dumps(dict(prefix=sys.prefix,base=sys.base_prefix,executable=sys.executable)))'
    check = json.loads(run(str(output / python), '-I', '-B', '-c', script))
    require(all(Path(check[k]).resolve().is_relative_to(output) for k in ('prefix', 'base', 'executable')),
            'Python selected a runtime outside the staged bundle')
    if applications:
        script = '''import json,importlib.metadata,gunicorn.app.wsgiapp,cffi,pycparser
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
key=Ed25519PrivateKey.generate();message=b'runtime smoke test'
key.public_key().verify(key.sign(message),message)
print(json.dumps({n:importlib.metadata.version(n) for n in ('gunicorn','cryptography','cffi','pycparser')}))'''
        package_versions = json.loads(run(str(output / python), '-I', '-B', '-c', script))
        require(package_versions == {n: p['version'] for n, p in applications['packages'].items()},
                'Isolated runtime package versions differ from pins')
    versions = {name: run(str(output / 'bin' / name), *(['--config-file=/dev/null'] if name == 'freshclam' else []),
                          '--version').strip()[:1000] for name in ('clamscan', 'freshclam', 'caddy')}
    result = dict(schema=1, producer='tsfp-host-runtime-v1', python=str(python), files=builder.records,
                  producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  copied_bytes=builder.total, payload_bytes=payload_bytes, native_images=len(builder.images), versions=versions,
                  isolated_python=check, production_installed=False, services_started=False)
    result['applications'] = applications
    with (output / 'runtime.json').open('x') as stream:
        json.dump(result, stream, sort_keys=True, indent=2)
    (output / 'runtime.json').chmod(0o600)
    return dict(output=str(output), copied_bytes=builder.total, native_images=len(builder.images),
                versions=versions, production_installed=False, services_started=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True)
    parser.add_argument('--wheelhouse', help='Offline directory containing exactly the pinned binary wheels')
    for name in ('python', 'clamscan', 'freshclam', 'caddy'):
        parser.add_argument('--' + name, default='/opt/homebrew/bin/' + ('python3.12' if name == 'python' else name))
    try:
        print(json.dumps(build(parser.parse_args()), indent=2))
        return 0
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print('Runtime staging: ' + str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
