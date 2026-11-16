"""WSGI contract and actual loopback HTTP creator-to-review workflow."""
import io
import json
import os
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
from mod_package import build_package, digest
from mod_publication import export_snapshot, validate_snapshot
from test_creator_service import service, account, package, Scanner, PASSWORD


def test_owner_invites_creator_once_without_exposing_secret_in_storage_or_audit(service):
    accounts,uploads,_=service;_,owner,_=account(accounts,'owner','admin');api=WorkshopAPI(accounts,'https://workshop.invalid',uploads=uploads)
    response=call(api,'POST','/v1/creator-invitations',{'username':'new-creator'},owner)
    assert response['status']==201;invitation=response['body']
    assert set(invitation)=={'username','invitation','expires'} and invitation['username']=='new-creator'
    assert invitation['expires']==accounts.now()+86400
    with accounts.repository.db() as db:
        row=db.execute('SELECT * FROM workshop_invites WHERE username=?',('new-creator',)).fetchone()
        assert row['role']=='creator' and row['account_id'] is None and row['token_hash']!=invitation['invitation']
        audit=[tuple(r) for r in db.execute('SELECT actor,action,target,note FROM workshop_audit')]
    assert ('user:owner','invite-created','new-creator','creator') in audit
    assert invitation['invitation'] not in repr(audit)
    redeemed=call(api,'POST','/v1/accounts/redeem',{'invitation':invitation['invitation'],'password':PASSWORD})
    assert redeemed['status']==201 and redeemed['body']['account']=={'username':'new-creator','role':'creator'}
    assert call(api,'POST','/v1/accounts/redeem',{'invitation':invitation['invitation'],'password':PASSWORD})['status']==401
    assert call(api,'POST','/v1/creator-invitations',{'username':'new-creator'},owner)['status']==409


@pytest.mark.parametrize('role',[None,'creator','moderator'])
def test_creator_invitation_api_requires_current_owner_role(service,role):
    accounts,uploads,_=service;token=None
    if role:_,token,_=account(accounts,'member',role)
    api=WorkshopAPI(accounts,'https://workshop.invalid',uploads=uploads)
    assert call(api,'POST','/v1/creator-invitations',{'username':'new-creator'},token)['status']==(403 if role else 401)
    assert call(api,'DELETE','/v1/creator-invitations/new-creator',token=token)['status']==(403 if role else 401)
    with accounts.repository.db() as db:assert not db.execute('SELECT 1 FROM workshop_invites WHERE username=?',('new-creator',)).fetchone()


@pytest.mark.parametrize('payload',[{'username':'guest','role':'admin'},{'username':'guest','role':'moderator'},{'username':'guest','account_id':1},{'username':'../guest'},{'username':'Aguest'},{}])
def test_owner_cannot_choose_privileged_roles_recover_accounts_or_alias_names_over_http(service,payload):
    accounts,uploads,_=service;_,owner,_=account(accounts,'owner','admin');api=WorkshopAPI(accounts,'https://workshop.invalid',uploads=uploads)
    assert call(api,'POST','/v1/creator-invitations',payload,owner)['status']==400
    with accounts.repository.db() as db:assert not db.execute('SELECT 1 FROM workshop_invites').fetchone()


def test_owner_can_cancel_only_new_creator_invites_and_replace_lost_delivery(service):
    accounts,uploads,_=service;_,owner,_=account(accounts,'owner','admin');api=WorkshopAPI(accounts,'https://workshop.invalid',uploads=uploads)
    first=call(api,'POST','/v1/creator-invitations',{'username':'new-creator'},owner)['body']
    assert call(api,'POST','/v1/creator-invitations',{'username':'new-creator'},owner)['status']==409
    assert call(api,'DELETE','/v1/creator-invitations/new-creator',token=owner)['status']==204
    assert call(api,'DELETE','/v1/creator-invitations/new-creator',token=owner)['status']==204
    assert call(api,'POST','/v1/accounts/redeem',{'invitation':first['invitation'],'password':PASSWORD})['status']==401
    second=call(api,'POST','/v1/creator-invitations',{'username':'new-creator'},owner)['body']
    assert second['invitation']!=first['invitation']
    for name,role in [('new-admin','admin'),('new-reviewer','moderator')]:
        accounts.create_invite(name,role)
        assert call(api,'DELETE','/v1/creator-invitations/'+name,token=owner)['status']==403
    account(accounts,'recovered')
    recovery=accounts.recovery_invite('recovered','Local operator recovery')
    assert call(api,'DELETE','/v1/creator-invitations/recovered',token=owner)['status']==403
    assert accounts.redeem(recovery,PASSWORD,'recovery-fixture')['token']


def test_creator_invites_recheck_revoked_or_demoted_owner_and_roll_back_audit_failure(service,monkeypatch):
    accounts,uploads,_=service;actor,owner,_=account(accounts,'owner','admin');api=WorkshopAPI(accounts,'https://workshop.invalid',uploads=uploads)
    original=accounts.audit
    def fail(*args):raise ServiceError(503,'Audit unavailable')
    monkeypatch.setattr(accounts,'audit',fail)
    assert call(api,'POST','/v1/creator-invitations',{'username':'new-creator'},owner)['status']==503
    with accounts.repository.db() as db:assert not db.execute('SELECT 1 FROM workshop_invites WHERE username=?',('new-creator',)).fetchone()
    monkeypatch.setattr(accounts,'audit',original)
    invitation=accounts.invite_creator(actor,'new-creator')
    with accounts.repository.db() as db:db.execute('UPDATE workshop_accounts SET role=? WHERE id=?',('creator',actor.id))
    with pytest.raises(ServiceError) as denied:accounts.invite_creator(actor,'other-creator')
    assert denied.value.status==403
    with pytest.raises(ServiceError) as denied:accounts.cancel_creator_invite(actor,'new-creator')
    assert denied.value.status==403
    accounts.logout(actor)
    assert call(api,'DELETE','/v1/creator-invitations/new-creator',token=owner)['status']==401
    assert accounts.redeem(invitation['invitation'],PASSWORD,'still-valid')['token']


def test_owner_creator_invite_rate_capacity_expiry_and_request_framing(service,monkeypatch):
    import mod_accounts
    accounts,uploads,clock=service;_,owner,_=account(accounts,'owner','admin');api=WorkshopAPI(accounts,'https://workshop.invalid',uploads=uploads)
    monkeypatch.setattr(mod_accounts,'MAX_ACCOUNTS',2)
    first=call(api,'POST','/v1/creator-invitations',{'username':'new-creator'},owner)['body']
    assert call(api,'POST','/v1/creator-invitations',{'username':'other-creator'},owner)['status']==503
    assert call(api,'DELETE','/v1/creator-invitations/new-creator',{'unexpected':True},owner)['status']==400
    assert call(api,'DELETE','/v1/creator-invitations/new-creator',token=owner,HTTP_TRANSFER_ENCODING='chunked')['status']==400
    assert call(api,'DELETE','/v1/creator-invitations/new-creator',token=owner,CONTENT_LENGTH='')['status']==204
    monkeypatch.setattr(mod_accounts,'MAX_ACCOUNTS',10000)
    for i in range(19):
        latest=call(api,'POST','/v1/creator-invitations',{'username':'guest-'+str(i)},owner)
        assert latest['status']==201
    assert call(api,'POST','/v1/creator-invitations',{'username':'excess-guest'},owner)['status']==429
    clock.value+=86400
    assert call(api,'POST','/v1/accounts/redeem',{'invitation':latest['body']['invitation'],'password':PASSWORD})['status']==401


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


def test_moderation_evidence_is_private_bound_and_reports_current_freshness(service, tmp_path):
    accounts, uploads, _ = service
    creator, token, _ = account(accounts)
    _, reviewer, _ = account(accounts, 'reviewer', 'moderator')
    api = WorkshopAPI(accounts, 'https://workshop.invalid', uploads=uploads)
    data = package(tmp_path)
    ticket = uploads.begin(creator, 'test-addon', '1.0.0', digest(data), len(data))
    route = '/v1/moderation/' + ticket['id'] + '/evidence'
    uploads.receive(creator, ticket['id'], io.BytesIO(data), len(data))
    assert call(api, path=route)['status'] == 401
    assert call(api, path=route, token=token)['status'] == 403
    assert call(api, path=route, token=reviewer)['status'] == 404
    uploads.process_next(Scanner())  # Explicit unit fixture, not real AV acceptance.
    with accounts.repository.db() as db:
        report = json.loads(db.execute('SELECT scan FROM releases').fetchone()[0])
        report['detail'] = '/private/host/path secret scanner stdout'
        db.execute('UPDATE releases SET scan=?', (json.dumps(report),))
    result = call(api, path=route, token=reviewer)
    assert result['status'] == 200
    evidence = result['body']
    assert set(evidence) == {'id', 'sha256', 'state', 'scan'}
    assert evidence['id'] == ticket['id'] and evidence['sha256'] == digest(data)
    assert evidence['state'] == 'pending-review' and evidence['scan']['fresh'] is True
    assert set(evidence['scan']) == {'engine','version','status','time','isolation','database_max_age_days','fresh'}
    assert 'secret' not in json.dumps(evidence) and '/private' not in json.dumps(evidence)
    assert call(api, path=route, token=reviewer, QUERY_STRING='after=0')['status'] == 400
    with accounts.repository.db() as db:
        report['time'] = 1
        db.execute('UPDATE releases SET scan=?', (json.dumps(report),))
    assert call(api, path=route, token=reviewer)['body']['scan']['fresh'] is False
    with accounts.repository.db() as db:
        report['version'] = 'forged\nscanner'
        db.execute('UPDATE releases SET scan=?', (json.dumps(report),))
    assert call(api, path=route, token=reviewer)['status'] == 503
    accounts.logout(accounts.authenticate(reviewer))
    assert call(api, path=route, token=reviewer)['status'] == 401


def test_lua_source_preview_binds_archive_role_payload_and_utf8_pages(service, tmp_path):
    accounts, uploads, _ = service
    creator, token, _ = account(accounts)
    _, reviewer, _ = account(accounts, 'reviewer', 'moderator')
    api = WorkshopAPI(accounts, 'https://workshop.invalid', uploads=uploads)
    package(tmp_path)
    source = tmp_path / 'test-addon-1.0.0-alice'
    text = '--' + 'a' * 65533 + '\u00e9\nreturn {label="x\u202ey"}\n'
    (source / 'scripts/main.lua').write_bytes(text.encode())
    archive = tmp_path / 'preview.zip'
    build_package(source, archive)
    data = archive.read_bytes()
    ticket = uploads.begin(creator, 'test-addon', '1.0.0', digest(data), len(data))
    uploads.receive(creator, ticket['id'], io.BytesIO(data), len(data))
    route = '/v1/moderation/' + ticket['id'] + '/source'
    request = dict(sha256=digest(data), path='scripts/main.lua', offset=0)
    assert call(api, 'POST', route, request)['status'] == 401
    assert call(api, 'POST', route, request, token)['status'] == 403
    assert call(api, 'POST', route, request, reviewer)['status'] == 404
    uploads.process_next(Scanner())  # Explicit unit fixture; native real AV is separate.
    first = call(api, 'POST', route, request, reviewer)
    assert first['status'] == 200 and first['body']['next_offset'] == 65535
    page = first['body']
    assert page['sha256'] == digest(data) and page['payload_sha256'] == digest(text.encode())
    assert page['size'] == len(text.encode()) and page['offset'] == 0
    second = call(api, 'POST', route, dict(request, offset=page['next_offset']), reviewer)
    assert second['status'] == 200
    assert page['text'] + second['body']['text'] == text
    assert second['body']['next_offset'] == len(text.encode())
    for patch, status in [({'sha256':'f'*64},409), ({'path':'../secret.lua'},400),
                          ({'path':'mod.json'},400), ({'path':'scripts/missing.lua'},400),
                          ({'offset':True},400), ({'offset':-1},400), ({'offset':len(text.encode())},400),
                          ({'offset':65536},409)]:
        assert call(api, 'POST', route, dict(request, **patch), reviewer)['status'] == status
    blob = accounts.repository.blob(digest(data))
    blob.write_bytes(b'corrupted private fixture')
    assert call(api, 'POST', route, request, reviewer)['status'] == 409
    assert accounts.repository.catalog() == []


def test_source_preview_limits_are_persistent_and_do_not_publish(service, tmp_path):
    accounts, uploads, _ = service
    creator, _, _ = account(accounts)
    reviewer, token, _ = account(accounts, 'reviewer', 'moderator')
    data = package(tmp_path)
    ticket = uploads.begin(creator, 'test-addon', '1.0.0', digest(data), len(data))
    uploads.receive(creator, ticket['id'], io.BytesIO(data), len(data))
    uploads.process_next(Scanner())
    api = WorkshopAPI(accounts, 'https://workshop.invalid', uploads=uploads)
    route = '/v1/moderation/' + ticket['id'] + '/source'
    request = dict(sha256=digest(data), path='scripts/main.lua', offset=0)
    with accounts.repository.db() as db:
        for _ in range(24):
            assert accounts.rate(db, 'source-preview-account', str(reviewer.id), 24, 60)
    assert call(api, 'POST', route, request, token)['status'] == 429
    assert call(api, 'POST', route, request, token)['status'] == 429
    # An already capped reviewer must not exhaust inspection for other staff.
    with accounts.repository.db() as db:
        for _ in range(48):
            assert accounts.rate(db, 'source-preview-global', 'all', 48, 60)
    assert accounts.repository.catalog() == []


@pytest.mark.skipif(os.name != 'posix', reason='Actual POSIX nofollow/ownership/mode contract')
@pytest.mark.parametrize('damage', ['symlink', 'hardlink', 'writable', 'fifo'])
def test_source_preview_refuses_unsafe_stored_blob(service, tmp_path, damage):
    accounts, uploads, _ = service
    creator, _, _ = account(accounts)
    _, token, _ = account(accounts, 'reviewer', 'moderator')
    data = package(tmp_path)
    ticket = uploads.begin(creator, 'test-addon', '1.0.0', digest(data), len(data))
    uploads.receive(creator, ticket['id'], io.BytesIO(data), len(data))
    uploads.process_next(Scanner())
    blob = accounts.repository.blob(digest(data))
    other = tmp_path / 'original-private-fixture.zip'
    if damage == 'symlink':
        other.write_bytes(blob.read_bytes())
        blob.unlink()
        blob.symlink_to(other)
    elif damage == 'hardlink':
        os.link(blob, other)
    elif damage == 'fifo':
        blob.unlink()
        os.mkfifo(blob, 0o600)
    else:
        blob.chmod(0o666)
    api = WorkshopAPI(accounts, 'https://workshop.invalid', uploads=uploads)
    response = call(api, 'POST', '/v1/moderation/' + ticket['id'] + '/source',
                    dict(sha256=digest(data), path='scripts/main.lua', offset=0), token)
    assert response['status'] == 503 and 'text' not in response['body']


def test_private_reports_target_prior_approval_and_do_not_mutate_release(service, tmp_path):
    accounts, uploads, _ = service
    creator, token, _ = account(accounts)
    _, other, _ = account(accounts, 'bravo')
    _, reviewer, _ = account(accounts, 'reviewer', 'moderator')
    api = WorkshopAPI(accounts, 'https://workshop.invalid', uploads=uploads)
    data = package(tmp_path)
    ticket = uploads.begin(creator, 'test-addon', '1.0.0', digest(data), len(data))
    uploads.receive(creator, ticket['id'], io.BytesIO(data), len(data))
    report = dict(sha256=digest(data), reason='broken', description='Original private issue description')
    assert call(api, 'POST', '/v1/reports', report)['status'] == 401
    assert call(api, 'POST', '/v1/reports', report, token)['status'] == 404
    uploads.process_next(Scanner())
    assert call(api, 'POST', '/v1/reports', report, token)['status'] == 404
    accounts.repository.approve(digest(data), 'Fixture review, not actual AV acceptance')
    assert call(api, 'POST', '/v1/reports', dict(report, reporter='bravo'), token)['status'] == 400
    receipt = call(api, 'POST', '/v1/reports', report, token)
    assert receipt['status'] == 201 and receipt['body']['state'] == 'open'
    row = receipt['body']
    assert row['reporter'] == 'alice' and row['sha256'] == digest(data)
    assert call(api, 'POST', '/v1/reports', report, token)['status'] == 409
    assert call(api, path='/v1/reports', token=other)['body'] == {'reports':[]}
    assert call(api, path='/v1/reports', token=token, QUERY_STRING='after='+str(row['cursor']))['body'] == {'reports':[]}
    assert call(api, path='/v1/moderation/reports', token=token)['status'] == 403
    queue = call(api, path='/v1/moderation/reports', token=reviewer)['body']['reports']
    assert len(queue) == 1 and queue[0]['id'] == row['id']
    linked = '/v1/moderation/by-digest/' + row['sha256']
    assert call(api, path=linked, token=token)['status'] == 403
    assert call(api, path=linked, token=reviewer)['body']['id'] == ticket['id']
    assert call(api, path='/v1/moderation/by-digest/'+'f'*64, token=reviewer)['status'] == 404
    assert call(api, path='/v1/moderation/report-history', token=token)['status'] == 403
    route = '/v1/moderation/reports/' + row['id']
    decision = dict(action='close', note='Investigated original fixture')
    assert call(api, 'POST', route, dict(action='approve', note='Not a release decision'), reviewer)['status'] == 400
    assert call(api, 'POST', route, decision, other)['status'] == 403
    assert call(api, 'POST', route, decision, reviewer)['body']['state'] == 'closed'
    assert call(api, path='/v1/moderation/reports', token=reviewer)['body'] == {'reports':[]}
    history = call(api, path='/v1/moderation/report-history', token=reviewer)['body']['reports']
    assert len(history) == 1 and history[0]['state'] == 'closed'
    assert call(api, path='/v1/moderation/report-history', token=reviewer, QUERY_STRING='after='+str(row['cursor']))['body'] == {'reports':[]}
    assert call(api, path='/v1/reports', token=token)['body']['reports'][0]['resolution'] == decision['note']
    assert len(accounts.repository.catalog()) == 1
    assert call(api, 'POST', route, dict(action='reopen', note='Follow up requested'), reviewer)['body']['state'] == 'open'
    assert len(accounts.repository.catalog()) == 1
    accounts.repository.revoke(digest(data), 'Separate fixture withdrawal')
    assert call(api, 'POST', '/v1/reports', report, other)['status'] == 201
    assert accounts.repository.catalog() == []
    with accounts.repository.db() as db:
        audit = db.execute("SELECT actor,action FROM workshop_audit WHERE action LIKE 'report-%' ORDER BY sequence").fetchall()
    assert [tuple(record) for record in audit] == [('user:alice','report-created'),('user:reviewer','report-close'),('user:reviewer','report-reopen'),('user:bravo','report-created')]


def test_reports_fail_closed_on_audit_and_persist_rate_admission(service, tmp_path, monkeypatch):
    accounts, uploads, _ = service
    actor, token, _ = account(accounts)
    _, reviewer, _ = account(accounts, 'reviewer', 'moderator')
    api = WorkshopAPI(accounts, 'https://workshop.invalid', uploads=uploads)
    data = package(tmp_path)
    ticket = uploads.begin(actor, 'test-addon', '1.0.0', digest(data), len(data))
    uploads.receive(actor, ticket['id'], io.BytesIO(data), len(data))
    uploads.process_next(Scanner())
    accounts.repository.approve(digest(data), 'Unit fixture only')
    report = dict(sha256=digest(data), reason='other', description='Private fixture report')
    original = accounts.audit
    def fail(db, actor, action, target, note=''):
        if action.startswith('report-'):
            raise ServiceError(503, 'Fixture audit refusal')
        return original(db, actor, action, target, note)
    monkeypatch.setattr(accounts, 'audit', fail)
    assert call(api, 'POST', '/v1/reports', report, token)['status'] == 503
    assert call(api, path='/v1/reports', token=token)['body'] == {'reports':[]}
    monkeypatch.setattr(accounts, 'audit', original)
    row = call(api, 'POST', '/v1/reports', report, token)['body']
    monkeypatch.setattr(accounts, 'audit', fail)
    assert call(api, 'POST', '/v1/moderation/reports/'+row['id'], dict(action='close', note='Blocked audit'), reviewer)['status'] == 503
    assert call(api, path='/v1/reports', token=token)['body']['reports'][0]['state'] == 'open'
    monkeypatch.setattr(accounts, 'audit', original)
    for _ in range(3):
        assert call(api, 'POST', '/v1/reports', report, token)['status'] == 409
    restarted = WorkshopAPI(accounts, 'https://workshop.invalid', uploads=uploads)
    assert call(restarted, 'POST', '/v1/reports', report, token)['status'] == 429
    with accounts.repository.db() as db:
        global_count = db.execute("SELECT count FROM workshop_rate WHERE scope='report-global'").fetchone()[0]
    for _ in range(5):
        assert call(restarted, 'POST', '/v1/reports', report, token)['status'] == 429
    with accounts.repository.db() as db:
        assert db.execute("SELECT count FROM workshop_rate WHERE scope='report-global'").fetchone()[0] == global_count
    assert call(api, 'POST', '/v1/reports', dict(report, description='\U0001f600'*1000), token)['status'] == 400
    assert len(accounts.repository.catalog()) == 1


def test_report_capacity_refuses_without_exposing_or_mutating_a_release(service, tmp_path, monkeypatch):
    import mod_reports
    accounts, uploads, _ = service
    actor, token, _ = account(accounts)
    data = package(tmp_path)
    ticket = uploads.begin(actor, 'test-addon', '1.0.0', digest(data), len(data))
    uploads.receive(actor, ticket['id'], io.BytesIO(data), len(data))
    uploads.process_next(Scanner())
    accounts.repository.approve(digest(data), 'Unit fixture only')
    api = WorkshopAPI(accounts, 'https://workshop.invalid', uploads=uploads)
    monkeypatch.setattr(mod_reports, 'MAX_REPORTS', 0)
    response = call(api, 'POST', '/v1/reports', dict(sha256=digest(data), reason='other', description='Private fixture'), token)
    assert response['status'] == 503
    assert call(api, path='/v1/reports', token=token)['body'] == {'reports':[]}
    assert len(accounts.repository.catalog()) == 1


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


@pytest.mark.parametrize('port', [8443, 10000])
def test_https_port_is_bound_to_host_and_browser_origin(service, port):
    accounts, _, _ = service
    origin = 'https://workshop.invalid:' + str(port)
    api = WorkshopAPI(accounts, origin)
    assert call(api, path='/healthz', HTTP_HOST='workshop.invalid:' + str(port), HTTP_ORIGIN=origin)['status'] == 200
    assert call(api, path='/healthz', HTTP_HOST='workshop.invalid', HTTP_ORIGIN=origin)['status'] == 403
    assert call(api, path='/healthz', HTTP_HOST='workshop.invalid:' + str(port), HTTP_ORIGIN='https://workshop.invalid')['status'] == 403


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
