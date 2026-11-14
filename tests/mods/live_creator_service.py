"""Mac acceptance: actual Gunicorn HTTP, accounts, isolated ClamAV and moderator workflow."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import secrets
import signal
import socket
import subprocess
import sys
import tempfile
import time
from urllib.error import HTTPError, URLError
from urllib.request import Request, build_opener, ProxyHandler

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/mods'))
from mod_accounts import Accounts
from mod_operator import issue_invitation
from mod_package import build_package
from mod_repository import ClamScanner, Repository
from mod_uploads import Uploads
from mod_publication import export_snapshot, validate_snapshot


def fixture(root, identity, script):
    source = root / identity
    (source / 'scripts').mkdir(parents=True)
    manifest = dict(schema=1, id=identity, version='1.0.0', name='Original HTTP fixture',
                    author='creator', description='Private creator acceptance', game='tsfp-us-45410066',
                    api='tsfp-mod-v1-draft', dependencies={}, files={})
    (source / 'mod.json').write_text(json.dumps(manifest))
    (source / 'scripts/main.lua').write_bytes(script)
    archive = root / (identity + '.zip')
    build_package(source, archive)
    return archive.read_bytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--database', required=True)
    parser.add_argument('--clamscan', default='/opt/homebrew/bin/clamscan')
    args = parser.parse_args()
    if sys.platform != 'darwin':
        parser.error('Run on the actual Mac workshop host')
    with tempfile.TemporaryDirectory(prefix='tsfp-creator-http-') as temporary:
        root = Path(temporary).resolve()
        storage = root / 'private'
        storage.mkdir(mode=0o700)
        repository = Repository(storage)
        accounts = Accounts(repository)
        uploads = Uploads(accounts)
        invites = {}
        for name, role in [('creator','creator'),('reviewer','moderator')]:
            result = issue_invitation(accounts, name, role)
            secret_file = Path(result['secret_file'])
            assert secret_file.stat().st_mode & 0o777 == 0o600
            invites[name] = json.loads(secret_file.read_text())['invitation']
        with socket.socket() as selected:
            selected.bind(('127.0.0.1',0))
            port = selected.getsockname()[1]
        origin = 'https://creator-acceptance.invalid'
        environment = {k:v for k,v in os.environ.items() if not k.startswith('TSFP_')}
        environment.update(TSFP_WORKSHOP_REPOSITORY=str(storage), TSFP_WORKSHOP_ORIGIN=origin)
        command = [sys.executable,'-m','gunicorn','--chdir',str(ROOT / 'tools/mods'),
                   '--config',str(ROOT / 'tools/mods/hosting/gunicorn.conf.py'),
                   '--bind','127.0.0.1:' + str(port),'mod_api:create_app()']
        client = build_opener(ProxyHandler({}))
        def request(method, path, body=None, token=None, binary=False, secure=True):
            encoded = body if binary else (json.dumps(body).encode() if body is not None else None)
            headers = {'Host':'creator-acceptance.invalid','Origin':origin,
                       'Content-Type':'application/zip' if binary else 'application/json'}
            if secure:
                headers['X-Forwarded-Proto'] = 'https'
            if token:
                headers['Authorization'] = 'Bearer ' + token
            with client.open(Request('http://127.0.0.1:' + str(port) + path,
                                     data=encoded,headers=headers,method=method), timeout=20) as response:
                assert response.headers['Cache-Control'] == 'no-store'
                return response.status, json.load(response)
        process = None
        with tempfile.TemporaryFile() as logs:
            try:
                process = subprocess.Popen(command,env=environment,stdout=logs,stderr=subprocess.STDOUT,
                                           start_new_session=True)
                deadline = time.monotonic() + 15
                while True:
                    assert process.poll() is None, 'Gunicorn terminated before readiness'
                    try:
                        assert request('GET','/healthz')[1]['ready']
                        break
                    except (URLError, HTTPError):
                        assert time.monotonic() < deadline, 'Gunicorn readiness timeout'
                        time.sleep(.1)
                try:
                    request('GET','/healthz',secure=False)
                except HTTPError as error:
                    assert error.code == 403
                else:
                    raise AssertionError('Untrusted HTTP scheme was accepted')
                tokens = {}
                password = secrets.token_urlsafe(32)
                for name in ('creator','reviewer'):
                    status, session = request('POST','/v1/accounts/redeem',
                                              {'invitation':invites[name],'password':password})
                    assert status == 201
                    tokens[name] = session['token']
                scanner = ClamScanner(args.clamscan,args.database)
                eicar = b'X5O!P%@AP[4\\PZX54(P^)7CC)7}$' + b'EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*'
                results = {}
                for identity, script, expected in [('clean-http',b'return {}\n','pending-review'),
                                                    ('eicar-http',eicar,'rejected')]:
                    data = fixture(root,identity,script)
                    sha = hashlib.sha256(data).hexdigest()
                    status, ticket = request('POST','/v1/uploads',
                        dict(id=identity,version='1.0.0',sha256=sha,size=len(data)),tokens['creator'])
                    assert status == 201
                    status, received = request('PUT','/v1/uploads/' + ticket['id'],data,tokens['creator'],True)
                    assert status == 202 and received['state'] == 'queued'
                    assert not any(item['sha256'] == sha for item in repository.catalog())
                    job = uploads.process_next(scanner)
                    assert job['state'] == expected, job
                    status, state = request('GET','/v1/uploads/' + ticket['id'],token=tokens['creator'])
                    assert status == 200 and state['state'] == expected
                    if expected == 'pending-review':
                        try:
                            request('POST','/v1/moderation/' + ticket['id'],
                                    {'action':'approve','note':'Creator must not approve'},tokens['creator'])
                        except HTTPError as error:
                            assert error.code == 403
                        else:
                            raise AssertionError('Creator approved own upload without moderator role')
                        _, reviewed = request('POST','/v1/moderation/' + ticket['id'],
                            {'action':'approve','note':'Reviewed original fixture'},tokens['reviewer'])
                        assert reviewed['state'] == 'approved'
                        public = root / 'approved-snapshot'
                        export_snapshot(repository,public)
                        validate_snapshot(public)
                        assert (public / 'v1/packages' / sha).read_bytes() == data
                        catalog = (public / 'v1/catalog').read_bytes()
                        for secret in [password,*tokens.values(),*invites.values()]:
                            assert secret.encode() not in catalog
                        _, revoked = request('POST','/v1/moderation/' + ticket['id'],
                            {'action':'revoke','note':'Acceptance revocation'},tokens['reviewer'])
                        assert revoked['state'] == 'revoked'
                    else:
                        try:
                            request('POST','/v1/moderation/' + ticket['id'],
                                    {'action':'approve','note':'Must be blocked'},tokens['reviewer'])
                        except HTTPError as error:
                            assert error.code in (400,409)
                        else:
                            raise AssertionError('Detected package was approved')
                    results[identity] = expected
                assert repository.catalog() == []
                print(json.dumps(dict(server='Gunicorn27.0.1 on private loopback', results=results,
                                      auth='passed', review='passed', revocation='passed',
                                      public_credentials='absent', internet='not exercised'),indent=2))
            finally:
                if process is not None and process.poll() is None:
                    os.killpg(process.pid,signal.SIGTERM)
                    try:
                        process.wait(timeout=20)
                    except subprocess.TimeoutExpired:
                        os.killpg(process.pid,signal.SIGKILL)
                        process.wait(timeout=10)
                logs.seek(0)
                detail = logs.read(65536).decode('utf-8',errors='replace')
                if process is not None and process.returncode not in (0,None):
                    print(detail,file=sys.stderr)
        with socket.socket() as probe:
            assert probe.connect_ex(('127.0.0.1',port)) != 0, 'Test server listener remained open'
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
