"""WSGI contract and actual loopback HTTP creator-to-review workflow."""
import io
import json
from pathlib import Path
import sys
import threading
from types import SimpleNamespace
from urllib.error import HTTPError
from urllib.request import Request, build_opener, ProxyHandler
from wsgiref.simple_server import make_server, WSGIRequestHandler
from wsgiref.util import setup_testing_defaults
from wsgiref.validate import validator

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
from mod_api import WorkshopAPI
from mod_accounts import ServiceError
from mod_package import digest
from mod_publication import export_snapshot, validate_snapshot
from test_creator_service import service, account, package, Scanner, PASSWORD


@pytest.mark.parametrize('platform,uid', [('posix',0),('nt',1000)])
def test_production_factory_refuses_root_and_unsupported_host(monkeypatch,platform,uid):
    import mod_api
    monkeypatch.setattr(mod_api,'os',SimpleNamespace(name=platform,geteuid=lambda:uid))
    with pytest.raises(ServiceError,match='unprivileged'):
        mod_api.create_app()


def call(api, method='GET', path='/v1/account', payload=None, token=None, raw=None, **overrides):
    environ = {}
    setup_testing_defaults(environ)
    body = raw if raw is not None else (json.dumps(payload).encode() if payload is not None else b'')
    environ.update(REQUEST_METHOD=method, PATH_INFO=path, HTTP_HOST='workshop.invalid',
                   QUERY_STRING='',
                   REMOTE_ADDR='127.0.0.1', CONTENT_TYPE='application/json', CONTENT_LENGTH=str(len(body)),
                   **{'wsgi.url_scheme':'https', 'wsgi.input':io.BytesIO(body)})
    if token:
        environ['HTTP_AUTHORIZATION'] = 'Bearer ' + token
    environ.update(overrides)
    response = {}
    def start(status, headers, exc_info=None):
        response['status'] = int(status.split()[0])
        response['headers'] = dict(headers)
    # A real server rejects malformed Content-Length before constructing WSGI
    # environ. Test the application's defensive fallback outside that validator.
    valid_framing = environ.get('CONTENT_LENGTH','').isdigit()
    iterator = (validator(api) if valid_framing else api)(environ, start)
    try:
        encoded = b''.join(iterator)
    finally:
        if hasattr(iterator, 'close'):
            iterator.close()
    response['body'] = json.loads(encoded) if encoded else None
    assert response['headers']['Cache-Control'] == 'no-store'
    assert response['headers']['X-Content-Type-Options'] == 'nosniff'
    return response


def test_wsgi_authentication_cookie_and_role_injection_are_rejected(service):
    accounts, uploads, _ = service
    api = WorkshopAPI(accounts, 'https://workshop.invalid')
    actor, token, _ = account(accounts)
    assert call(api, token=token)['body'] == {'username':'alice','role':'creator'}
    assert call(api)['status'] == 401
    assert call(api, HTTP_COOKIE='token=' + token)['status'] == 401
    assert call(api, token=token, path='/v1/moderation')['status'] == 403
    invitation = accounts.create_invite('bravo')
    result = call(api, 'POST', '/v1/accounts/redeem',
                  {'invitation':invitation,'password':PASSWORD,'role':'admin'})
    assert result['status'] == 400
    assert call(api, 'DELETE', '/v1/sessions/current', token=token)['status'] == 204
    assert call(api, token=token)['status'] == 401


@pytest.mark.parametrize('overrides', [
    {'HTTP_HOST':'attacker.invalid'}, {'wsgi.url_scheme':'http'},
    {'HTTP_ORIGIN':'https://attacker.invalid'}, {'HTTP_ORIGIN':'null'},
    {'HTTP_SEC_FETCH_SITE':'cross-site'}, {'QUERY_STRING':'token=not-an-authentication-channel'}
])
def test_wrong_origin_transport_and_query_are_denied(service, overrides):
    accounts, _, _ = service
    api = WorkshopAPI(accounts, 'https://workshop.invalid')
    result = call(api, path='/healthz', **overrides)
    assert result['status'] in (400,403)
    assert 'Access-Control-Allow-Origin' not in result['headers']


@pytest.mark.parametrize('raw,overrides', [
    (b'{"username":"alice","username":"bravo","password":"fixture"}', {}),
    (b'{}', {'CONTENT_LENGTH':'2, 2'}), (b'{}', {'CONTENT_LENGTH':'-1'}),
    (b'{}', {'HTTP_TRANSFER_ENCODING':'chunked'}),
    (b'{}', {'CONTENT_TYPE':'text/plain'}), (b'{}', {'CONTENT_LENGTH':'20000'}),
    (b'{}', {'CONTENT_LENGTH':'10'}),
])
def test_json_limits_and_ambiguous_framing_fail_before_auth_hash(service, raw, overrides):
    accounts, _, _ = service
    result = call(WorkshopAPI(accounts, 'https://workshop.invalid'), 'POST', '/v1/sessions', raw=raw, **overrides)
    assert result['status'] in (400,411,413,415)


def test_upload_json_metadata_private_status_and_no_quarantine_download_route(service, tmp_path):
    accounts, uploads, _ = service
    api = WorkshopAPI(accounts, 'https://workshop.invalid')
    actor, token, _ = account(accounts)
    data = package(tmp_path)
    metadata = {'id':'test-addon','version':'1.0.0','sha256':digest(data),'size':len(data)}
    ticket = call(api, 'POST', '/v1/uploads', metadata, token)['body']
    path = '/v1/uploads/' + ticket['id']
    uploaded = call(api, 'PUT', path, raw=data, token=token, CONTENT_TYPE='application/zip')
    assert uploaded['status'] == 202 and uploaded['body']['state'] == 'queued'
    listing = call(api, path='/v1/uploads', token=token)
    assert listing['body']['uploads'][0]['id'] == ticket['id']
    assert not ({'lease','account_id','path','password_hash'} & set(uploaded['body']))
    assert call(api, path='/incoming/' + ticket['id'] + '.ready', token=token)['status'] == 404
    assert call(api, path='/v1/packages/' + digest(data), token=token)['status'] == 404
    assert accounts.repository.catalog() == []


def test_real_loopback_http_upload_review_and_public_snapshot_exclude_credentials(service, tmp_path):
    accounts, uploads, _ = service
    invite = accounts.create_invite('alice')
    _, reviewer_token, _ = account(accounts, 'reviewer', 'moderator')
    class Quiet(WSGIRequestHandler):
        def log_message(self, *args):
            pass
    server = make_server('127.0.0.1', 0, lambda *_: [], handler_class=Quiet)
    origin = 'http://127.0.0.1:' + str(server.server_port)
    server.set_app(WorkshopAPI(accounts, origin, local=True, uploads=uploads))
    worker = threading.Thread(target=server.serve_forever)
    worker.start()
    client = build_opener(ProxyHandler({}))
    def request(method, path, body=None, token=None, binary=False):
        encoded = body if binary else (json.dumps(body).encode() if body is not None else None)
        headers = {'Content-Type':'application/zip' if binary else 'application/json'}
        if token:
            headers['Authorization'] = 'Bearer ' + token
        with client.open(Request(origin + path, data=encoded, headers=headers, method=method), timeout=15) as response:
            return response.status, json.load(response)
    try:
        _, session = request('POST','/v1/accounts/redeem',{'invitation':invite,'password':PASSWORD})
        token = session['token']
        data = package(tmp_path)
        _, ticket = request('POST','/v1/uploads',{'id':'test-addon','version':'1.0.0','sha256':digest(data),'size':len(data)},token)
        status, received = request('PUT','/v1/uploads/' + ticket['id'],data,token,True)
        assert status == 202 and received['state'] == 'queued'
        with pytest.raises(HTTPError) as error:
            request('POST','/v1/moderation/' + ticket['id'],{'action':'approve','note':'Creator cannot review'},token)
        assert error.value.code == 403
        assert accounts.repository.catalog() == []
        uploads.process_next(Scanner())
        _, approved = request('POST','/v1/moderation/' + ticket['id'],{'action':'approve','note':'Reviewed original source'},reviewer_token)
        assert approved['state'] == 'approved'
        snapshot = tmp_path / 'public'
        export_snapshot(accounts.repository, snapshot)
        validate_snapshot(snapshot)
        assert (snapshot / 'v1/packages' / digest(data)).read_bytes() == data
        public = (snapshot / 'v1/catalog').read_bytes()
        for secret in (invite, token, reviewer_token, PASSWORD):
            assert secret.encode() not in public
        assert b'password_hash' not in public and b'workshop_audit' not in public
        _, revoked = request('POST','/v1/moderation/' + ticket['id'],{'action':'revoke','note':'Acceptance revocation'},reviewer_token)
        assert revoked['state'] == 'revoked' and accounts.repository.catalog() == []
    finally:
        server.shutdown()
        worker.join(timeout=10)
        server.server_close()
        assert not worker.is_alive()
