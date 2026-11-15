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
    identity.create_record('Users', dict(UserShell=['/usr/bin/false'], Password=['*'],
                                       AuthenticationAuthority=identity.DISABLED, UniqueID=['600']))
    assert all(call[:3] == ('/usr/bin/dscl', '.', '-create') for call in calls)
    assert calls[1][4:] == ('UserShell', '/usr/bin/false')
    assert calls[2][4:] == ('Password', '*')
    assert calls[3][4:] == ('AuthenticationAuthority', ';DisabledUser;', ';DisabledTags;SecureToken')
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


def test_existing_os_generated_uuid_is_preserved_without_writing_it(monkeypatch):
    value = '02436ED4-9078-40C5-8658-4F13145F15B4'
    monkeypatch.setattr(identity, 'attributes', lambda *_: {'GeneratedUID': [value]})
    monkeypatch.setattr(identity, 'command', lambda *_: pytest.fail('Existing UUID was overwritten'))
    assert identity.generated_uuid('Users', 'F2AFF881-19ED-4D4D-8C7E-DDBD2AD00FA3') == value


def test_directory_failure_identifies_operation_and_exit_without_dumping_arguments(monkeypatch):
    from types import SimpleNamespace
    monkeypatch.setattr(identity.subprocess, 'run', lambda *_a, **_k: SimpleNamespace(returncode=70, stdout=b'', stderr=b'Operation not permitted'))
    with pytest.raises(identity.ProvisionError, match='dscl create GeneratedUID, exit 70.*Operation not permitted'):
        identity.command('/usr/bin/dscl', '.', '-create', '/Users/_tsfpworkshop', 'GeneratedUID', 'fixture-uuid')


repair_spec = importlib.util.spec_from_file_location('repair_identity', ROOT / 'tools/mods/hosting/repair-service-mac.py')
repair = importlib.util.module_from_spec(repair_spec)
repair_spec.loader.exec_module(repair)


@pytest.fixture
def partial():
    receipt = identity.plan({}, {})
    receipt.update(phase='preparing', user_uuid='F2AFF881-19ED-4D4D-8C7E-DDBD2AD00FA3',
                   group_uuid='36538C5E-34FA-419F-88D8-E589F3D519BF')
    user = dict(UniqueID=['600'], PrimaryGroupID=['600'], UserShell=['/usr/bin/false'],
                NFSHomeDirectory=[str(identity.ROOT / 'data')], GeneratedUID=['02436ED4-9078-40C5-8658-4F13145F15B4'],
                AuthenticationAuthority=None, IsHidden=None, RealName=None)
    return receipt, user


def test_explicit_repair_plan_requires_exact_observed_partial_record(partial):
    receipt, user = partial
    result = repair.assess(receipt, user, {identity.ACCOUNT: 600}, {}, user['GeneratedUID'][0])
    assert result['user_uuid'] == user['GeneratedUID'][0] and result['uid'] == 600
    assert receipt['user_uuid'] != result['user_uuid'] and not result['services_started']


@pytest.mark.parametrize('field,value', [('GeneratedUID', ['FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF']),
                                         ('UserShell', ['/bin/zsh']), ('RealName', ['Unrecognized account'])])
def test_repair_preserves_changed_accounts(partial, field, value):
    receipt, user = partial
    observed = user['GeneratedUID'][0]
    user[field] = value
    with pytest.raises(repair.identity.ProvisionError, match='differs'):
        repair.assess(receipt, user, {identity.ACCOUNT: 600}, {}, observed)


def test_repair_preserves_existing_groups_and_identity_collisions(partial):
    receipt, user = partial
    for groups in ({'_tsfpworkshop': 601}, {'unrelated': 600}):
        with pytest.raises(repair.identity.ProvisionError, match='existing group'):
            repair.assess(receipt, user, {identity.ACCOUNT: 600}, groups, user['GeneratedUID'][0])


def test_repair_sets_disable_flags_after_password_and_never_replaces_user_uuid(partial, monkeypatch):
    receipt, _ = partial
    calls = []
    monkeypatch.setattr(repair.identity, 'command', lambda *args: calls.append(args))
    monkeypatch.setattr(repair.identity, 'generated_uuid', lambda *_: receipt['group_uuid'])
    repair.finish_records(receipt)
    user_calls = [call for call in calls if call[3] == '/Users/' + identity.ACCOUNT]
    assert [call[4] for call in user_calls] == ['Password', 'RealName', 'IsHidden', 'AuthenticationAuthority']
    assert user_calls[-1][5:] == tuple(identity.DISABLED)


def test_repair_checks_privilege_before_reading_or_mutating_root(monkeypatch):
    monkeypatch.setattr(repair.sys, 'platform', 'darwin')
    monkeypatch.setattr(repair.os, 'geteuid', lambda: 501, raising=False)
    monkeypatch.setattr(repair.identity, 'unlinked', lambda *_: pytest.fail('Unprivileged repair reached root inspection'))
    with pytest.raises(repair.identity.ProvisionError, match='administrator sudo'):
        repair.repair('02436ED4-9078-40C5-8658-4F13145F15B4', True)
