"""Freeze the active Windows CPython and committed operator source, new-only."""
import ast
import hashlib
import importlib.metadata
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys

from mod_package import require

BOOTSTRAP = 'tools/mods/hosting/frozen-backup-worker.py'
ENTRY = 'tools/mods/hosting/backup-receiver-worker.py'
MAX_FILE = 32 * 1024**2
MAX_TOTAL = 128 * 1024**2
PRUNE = {'site-packages', '__pycache__', 'test', 'tests', 'idlelib', 'ensurepip', 'tkinter', 'turtledemo'}


def bootstrap():
    spec = importlib.util.spec_from_file_location('frozen_backup_admission', Path(__file__).parent / 'hosting/frozen-backup-worker.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def source_files(source):
    source = Path(source).absolute()
    def git(*args):
        return subprocess.check_output(['git', '-C', str(source), *args], timeout=30)
    require(not git('status', '--porcelain').strip(), 'Commit operator source before freezing')
    revision = git('rev-parse', 'HEAD').decode('ascii').strip()
    def blob(path):
        require(0 < int(git('cat-file', '-s', revision + ':' + path)) <= 1024**2, 'Operator source exceeds budget')
        return git('show', revision + ':' + path)
    payload = {'bootstrap.py': blob(BOOTSTRAP),
               'source/hosting/private-recovery-windows.ps1': blob('tools/mods/hosting/private-recovery-windows.ps1'),
               'source/hosting/schedule-backup-receiver-windows.ps1': blob('tools/mods/hosting/schedule-backup-receiver-windows.ps1')}
    # Closed local Python imports; no entire repository, assets or personal data.
    pending = [ENTRY]
    while pending:
        name = pending.pop()
        output = 'source/' + name.removeprefix('tools/mods/')
        if output in payload:
            continue
        data = blob(name)
        payload[output] = data
        require(len(payload) <= 64, 'Operator source closure exceeds budget')
        for node in ast.walk(ast.parse(data)):
            imports = [a.name for a in node.names] if isinstance(node, ast.Import) else [node.module or ''] if isinstance(node, ast.ImportFrom) else []
            for imported in imports:
                if imported.startswith('mod_'):
                    require(imported.replace('_', '').isalnum() and '.' not in imported, 'Unknown local operator import')
                    pending.append('tools/mods/' + imported + '.py')
    require(not git('status', '--porcelain').strip() and git('rev-parse', 'HEAD').decode('ascii').strip() == revision,
            'Operator source changed during preparation')
    return revision, payload


def payload_files():
    require(os.name == 'nt' and sys.version_info[:2] == (3, 12) and sys.maxsize > 2**32,
            'Use the project Windows x64 Python3.12 environment')
    base = Path(sys.base_prefix)
    files = {}
    for name in ('python.exe', 'pythonw.exe', 'python3.dll', 'python312.dll', 'vcruntime140.dll',
                 'vcruntime140_1.dll', 'LICENSE.txt'):
        files['runtime/' + name] = base / name
    for directory in ('Lib', 'DLLs'):
        for path in (base / directory).rglob('*'):
            relative = path.relative_to(base)
            if any(part in PRUNE for part in relative.parts) or path.suffix in ('.pyc', '.pyo'):
                continue
            if path.is_file():
                files['runtime/' + relative.as_posix()] = path
    for name in ('cryptography', 'cffi', 'pycparser', '_cffi_backend'):
        spec = importlib.util.find_spec(name)
        require(spec is not None and spec.origin, 'Required operator crypto dependency is unavailable')
        path = Path(spec.origin)
        if spec.submodule_search_locations is None:
            files['packages/' + path.name] = path
        else:
            for child in path.parent.rglob('*'):
                if child.is_file() and '__pycache__' not in child.parts and child.suffix not in ('.pyc', '.pyo'):
                    files['packages/' + name + '/' + child.relative_to(path.parent).as_posix()] = child
        if name != '_cffi_backend':
            distribution = importlib.metadata.distribution(name)
            licenses = 0
            for member in distribution.files or ():
                parts = member.parts
                if not parts or not parts[0].endswith('.dist-info'):
                    continue
                if member.name in ('METADATA', 'WHEEL') or any('license' in part.lower() or 'copying' in part.lower() for part in parts[1:]):
                    files['packages/' + member.as_posix()] = Path(distribution.locate_file(member))
                    licenses += int(any('license' in part.lower() or 'copying' in part.lower() for part in parts[1:]))
            require(licenses > 0, 'Preserve each operator dependency license')
    return files


def prepare(source, destination):
    require(os.name == 'nt', 'Freeze the operator runtime on Windows')
    from mod_windows_recovery import private
    destination = Path(destination).absolute()
    private(destination.parent)
    require(not destination.exists() and not destination.is_symlink(), 'Preserving an existing operator runtime')
    revision, source_payload = source_files(source)
    installed = payload_files()
    require(not set(installed) & set(source_payload), 'Runtime source inventory collides')
    require(len(installed) + len(source_payload) + 1 <= 4096, 'Runtime file budget exceeded')
    require(shutil.disk_usage(destination.parent).free >= MAX_TOTAL + 2 * 1024**3, 'Insufficient runtime staging headroom')
    private(destination, create=True)
    admission = bootstrap()
    records = {}
    total = 0
    for name in sorted(set(installed) | set(source_payload) | {'runtime/python312._pth'}):
        if name in installed:
            path = installed[name]
            for ancestor in path.parents:
                admission.plain(ancestor, directory=True)
            before = admission.fingerprint(path, MAX_FILE)
            with path.open('rb') as stream:
                data = stream.read(MAX_FILE + 1)
            require(len(data) == before['bytes'] and hashlib.sha256(data).hexdigest() == before['sha256'],
                    'Installed operator dependency changed')
        else:
            data = b'Lib\nDLLs\n.\n' if name == 'runtime/python312._pth' else source_payload[name]
        total += len(data)
        require(len(data) <= MAX_FILE and total <= MAX_TOTAL, 'Operator runtime payload exceeds budget')
        path = destination / name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open('xb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        records[name] = dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
    manifest = admission.canonical(dict(schema=1, type=admission.TYPE, source_revision=revision,
                                        python_version=list(sys.version_info[:3]), files=records))
    require(len(manifest) <= admission.MAX_MANIFEST, 'Runtime manifest exceeds budget')
    private(destination, tree=True)
    with (destination / admission.MANIFEST).open('xb') as stream:
        stream.write(manifest)
        stream.flush()
        os.fsync(stream.fileno())
    fingerprint = hashlib.sha256(manifest).hexdigest()
    admission.verify(destination, fingerprint)
    return dict(schema=1, phase='backup-runtime-frozen', runtime=str(destination), runtime_sha256=fingerprint,
                source_revision=revision, files=len(records), bytes=total, services_started=False)
