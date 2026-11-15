"""Disposable real WSGI backend for native-client wire checks, over private pipes.

This helper opens no socket and does not scan/approve/publish any package. Its
invitation and password records belong only to the temporary test repository.
"""
import base64
import io
import json
from pathlib import Path
import sys
from urllib.parse import urlsplit

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
from mod_accounts import Accounts
from mod_api import WorkshopAPI
from mod_package import build_package
from mod_repository import Repository

MAX_FRAME = 2 * 1024 * 1024


def emit(value):
    wire = json.dumps(value, ensure_ascii=True, separators=(',', ':'))
    if len(wire) > MAX_FRAME:
        raise ValueError('Bridge response limit')
    print(wire, flush=True)


def package(root, author, version):
    source = root / ('source-' + version)
    (source / 'scripts').mkdir(parents=True)
    (source / 'scripts/main.lua').write_text('return {api=1}\n')
    (source / 'mod.json').write_text(json.dumps(dict(
        schema=1, id='native-original', name='Original native wire fixture',
        version=version, author=author, description='Asset-free test source',
        game='tsfp-us-45410066', api='tsfp-mod-v1-draft', dependencies={}, files={})))
    output = root / ('original-' + version + '.zip')
    build_package(source, output)
    return str(output)


def main():
    if sys.version_info < (3, 12) or len(sys.argv) != 4:
        raise RuntimeError('Use the pinned developer Python')
    root, origin, now = Path(sys.argv[1]), sys.argv[2], int(sys.argv[3])
    root.mkdir(exist_ok=False)
    accounts = Accounts(Repository(root / 'repository'), lambda: now)
    api = WorkshopAPI(accounts, origin)
    emit(dict(ready=True, invitation=accounts.create_invite('native-creator'),
              other_invitation=accounts.create_invite('other-creator'),
              package=package(root, 'native-creator', '1.0.0'),
              other_package=package(root, 'other-creator', '2.0.0')))
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
