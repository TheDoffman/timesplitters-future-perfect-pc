"""Disposable real WSGI backend for native-client wire checks, over private pipes.

This helper opens no socket. Its explicit Mac scanner mode uses the real sealed
engine; no mode approves/publishes. Credentials belong to a temporary repository.
"""
import base64
import hashlib
import io
import json
import os
from pathlib import Path
import stat
import sys
from urllib.parse import urlsplit

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
from mod_accounts import Accounts
from mod_api import WorkshopAPI
from mod_package import build_package
from mod_repository import ClamScanner, Repository
from mod_runtime_files import runtime_access

MAX_FRAME = 2 * 1024 * 1024


def emit(value):
    wire = json.dumps(value, ensure_ascii=True, separators=(',', ':'))
    if len(wire) > MAX_FRAME:
        raise ValueError('Bridge response limit')
    print(wire, flush=True)


def package(root, author, version, identity='native-original', script=b'return {api=1}\n'):
    source = root / ('source-' + identity + '-' + version)
    (source / 'scripts').mkdir(parents=True)
    (source / 'scripts/main.lua').write_bytes(script)
    (source / 'mod.json').write_text(json.dumps(dict(
        schema=1, id=identity, name='Original native wire fixture',
        version=version, author=author, description='Asset-free test source',
        game='tsfp-us-45410066', api='tsfp-mod-v1-draft', dependencies={}, files={})))
    output = root / (identity + '-' + version + '.zip')
    build_package(source, output)
    return str(output)


def main():
    if sys.version_info < (3, 12) or len(sys.argv) != 4:
        raise RuntimeError('Use the pinned developer Python')
    root, origin, now = Path(sys.argv[1]), sys.argv[2], int(sys.argv[3])
    root.mkdir(exist_ok=False)
    clock = [now]
    accounts = Accounts(Repository(root / 'repository'), lambda: clock[0])
    api = WorkshopAPI(accounts, origin)
    scanning = os.environ.get('TSFP_TEST_CREATOR_REAL_SCAN') == '1'
    scan_packages, scanner_config = {}, None
    if scanning:
        if sys.platform != 'darwin':
            raise RuntimeError('Real scanner acceptance requires macOS')
        config_path = Path(os.environ['TSFP_TEST_CREATOR_SCANNER_CONFIG'])
        info = config_path.lstat()
        if not stat.S_ISREG(info.st_mode) or info.st_uid != os.getuid() or info.st_nlink != 1 or info.st_mode & 0o077 or info.st_size > 8192:
            raise RuntimeError('Use an owner-private scanner acceptance configuration')
        scanner_config = json.loads(config_path.read_bytes())
        if set(scanner_config) != {'engine', 'database', 'runtime_receipt_sha256'}:
            raise RuntimeError('Invalid scanner acceptance configuration')
        engine = Path(scanner_config['engine']).resolve(strict=True)
        receipt = engine.parent.parent / 'runtime.json'
        if hashlib.sha256(receipt.read_bytes()).hexdigest() != scanner_config['runtime_receipt_sha256'] or runtime_access(engine) is None:
            raise RuntimeError('Scanner acceptance runtime changed')
        # EICAR is a harmless AV test string. It is never executed as Lua/code.
        eicar = b'X5O!P%@AP[4\\PZX54(P^)7CC)7}$' + b'EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*'
        scan_packages = {kind: package(root, 'native-creator', '1.0.0', 'native-' + kind,
                                      eicar if kind == 'eicar' else b'return {api=1}\n')
                         for kind in ('clean', 'eicar', 'empty', 'missing')}
        (root / 'empty-definitions').mkdir(mode=0o700)
    emit(dict(ready=True, invitation=accounts.create_invite('native-creator'),
              other_invitation=accounts.create_invite('other-creator'),
              package=package(root, 'native-creator', '1.0.0'),
              other_package=package(root, 'other-creator', '2.0.0'), scan_packages=scan_packages))
    while True:
        line = sys.stdin.buffer.readline(MAX_FRAME + 1)
        if not line:
            return
        if len(line) > MAX_FRAME:
            raise ValueError('Bridge request limit')
        request = json.loads(line)
        if request == {'stop': True}:
            emit({'stopped': True})
            return
        if scanning and request == {'advance': 301}:
            clock[0] += 301  # Test account/retry clock only; never the OS/AV clock.
            emit({'now': clock[0]})
            continue
        if scanning and set(request) == {'scan_next'}:
            kind = request['scan_next']
            if kind not in ('normal', 'empty', 'missing'):
                raise ValueError('Unknown scanner acceptance case')
            engine = scanner_config['engine'] if kind != 'missing' else str(root / 'missing-engine')
            database = scanner_config['database'] if kind != 'empty' else root / 'empty-definitions'
            job = api.uploads.process_next(ClamScanner(engine, database))
            if job is None:
                raise RuntimeError('No queued scanner acceptance job')
            with accounts.repository.db() as db:
                sha = db.execute('SELECT sha256 FROM workshop_uploads WHERE id=?', (job['id'],)).fetchone()[0]
            emit({'job': job, 'scan': accounts.repository.get(sha)['scan'],
                  'catalog_count': len(accounts.repository.catalog())})
            continue
        body = base64.b64decode(request['body'], validate=True)
        environ = dict(REQUEST_METHOD=request['method'], PATH_INFO=request['path'],
                       QUERY_STRING=request['query'], HTTP_HOST=request['authority'],
                       HTTP_ORIGIN=request['origin'], REMOTE_ADDR='native-pipe-fixture',
                       CONTENT_LENGTH=str(request['length']), CONTENT_TYPE=request['content_type'],
                       HTTP_AUTHORIZATION=request['authorization'],
                       **{'wsgi.url_scheme': urlsplit(origin).scheme,
                          'wsgi.input': io.BytesIO(body)})
        headers = {}
        status = []
        def start_response(value, supplied):
            status.append(int(value.split(' ', 1)[0]))
            headers.update(supplied)
        result = b''.join(api(environ, start_response))
        emit(dict(status=status[0], content_type=headers.get('Content-Type', ''),
                  body=base64.b64encode(result).decode('ascii')))


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        # Never put request bodies, invitations or bearer values in build logs.
        print('Native backend bridge failed: ' + type(error).__name__, file=sys.stderr)
        raise SystemExit(2)
