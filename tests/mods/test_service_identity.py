"""Read-only identity planning and preservation gates; no system users created."""
import importlib.util
from pathlib import Path
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('service_identity', ROOT / 'tools/mods/hosting/provision-service-mac.py')
identity = importlib.util.module_from_spec(spec)
spec.loader.exec_module(identity)


def test_plan_uses_an_unused_user_and_group_number_and_exposes_no_login():
    users = identity.parse_inventory('root 0\nnobody -2\nowner 501\nother 600\n')
    groups = identity.parse_inventory('wheel 0\nadmin 80\nother-group 601\n')
    result = identity.plan(users, groups)
    assert result['uid'] == result['gid'] == 602
    assert result['account'] == '_tsfpworkshop'
    assert not result['administrator'] and not result['password_login'] and not result['interactive_shell']
    assert not result['services_started'] and not result['public_endpoint_started']


@pytest.mark.parametrize('users,groups', [({'_TSFPWORKSHOP': 700}, {}), ({}, {'_tsfpworkshop': 700})])
def test_existing_service_identity_is_never_adopted_without_receipt(users, groups):
    with pytest.raises(identity.ProvisionError, match='Preserving'):
        identity.plan(users, groups)


@pytest.mark.parametrize('raw', ['owner not-a-number', 'owner 501\nowner 502', 'missing-number'])
def test_malformed_inventory_does_not_select_an_identity(raw):
    with pytest.raises(identity.ProvisionError):
        identity.parse_inventory(raw)


def test_directory_record_creation_keeps_login_disabled_before_assigning_uid(monkeypatch):
    calls = []
    monkeypatch.setattr(identity, 'command', lambda *args: calls.append(args))
    identity.create_record('Users', dict(UserShell=['/usr/bin/false'], AuthenticationAuthority=identity.DISABLED,
                                       Password=['*'], UniqueID=['600']))
    assert all(call[:3] == ('/usr/bin/dscl', '.', '-create') for call in calls)
    assert calls[1][4:] == ('UserShell', '/usr/bin/false')
    assert calls[2][4:] == ('AuthenticationAuthority', ';DisabledUser;', ';DisabledTags;SecureToken')
    assert calls[-1][4:] == ('UniqueID', '600')


def test_unknown_existing_directory_is_preserved(tmp_path, monkeypatch):
    canary = tmp_path / 'personal.txt'
    canary.write_text('preserve')
    monkeypatch.setattr(identity, 'ROOT', tmp_path)
    with pytest.raises(OSError):
        identity.existing()
    assert canary.read_text() == 'preserve'


def test_incomplete_receipt_never_resumes_directory_mutation(monkeypatch):
    monkeypatch.setattr(identity, 'command', lambda *_: pytest.fail('An incomplete receipt performed directory operations'))
    with pytest.raises(identity.ProvisionError, match='Incomplete'):
        identity.verify(dict(schema=1, producer=identity.PRODUCER, phase='preparing'))


def test_missing_admin_privilege_is_checked_before_mutations(monkeypatch):
    monkeypatch.setattr(identity.sys, 'platform', 'darwin')
    monkeypatch.setattr(identity.os, 'geteuid', lambda: 501, raising=False)
    monkeypatch.setattr(identity, 'command', lambda *_: pytest.fail('Unauthenticated directory mutation'))
    with pytest.raises(identity.ProvisionError, match='administrator sudo'):
        identity.apply()
