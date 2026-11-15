"""Inspect or explicitly finish the known pre-group identity failure; never starts services."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import stat
import sys
import uuid

spec = importlib.util.spec_from_file_location('service_identity', Path(__file__).with_name('provision-service-mac.py'))
identity = importlib.util.module_from_spec(spec)
spec.loader.exec_module(identity)
INTENT = 'identity-repair-intent.json'
NAMES = ('UniqueID', 'PrimaryGroupID', 'UserShell', 'NFSHomeDirectory', 'GeneratedUID',
         'AuthenticationAuthority', 'IsHidden', 'RealName')


def assess(receipt, user, users, groups, expected_uuid):
    need = identity.require
    fields = {'schema', 'producer', 'account', 'uid', 'gid', 'root', 'password_login',
              'interactive_shell', 'administrator', 'services_started', 'public_endpoint_started',
              'phase', 'user_uuid', 'group_uuid'}
    need(isinstance(receipt, dict) and set(receipt) == fields and type(receipt['schema']) is int
         and receipt['schema'] == 1 and receipt['producer'] == identity.PRODUCER
         and receipt['phase'] == 'preparing' and receipt['account'] == identity.ACCOUNT
         and receipt['root'] == str(identity.ROOT), 'Unknown partial identity receipt; preserving it')
    for key in ('password_login', 'interactive_shell', 'administrator', 'services_started', 'public_endpoint_started'):
        need(receipt[key] is False, 'Unexpected partial identity capabilities')
    number = receipt['uid']
    need(type(number) is int and 600 <= number < 10000 and receipt['gid'] == number,
         'Invalid partial numeric identity')
    for value in (expected_uuid, receipt['user_uuid'], receipt['group_uuid']):
        need(isinstance(value, str), 'Invalid identity UUID')
        need(str(uuid.UUID(value)).upper() == value, 'Invalid identity UUID')
    need(user.get('AuthenticationAuthority') in (None, identity.DISABLED),
         'Unexpected partial authentication policy; preserving account')
    expected = dict(UniqueID=[str(number)], PrimaryGroupID=[str(number)], UserShell=['/usr/bin/false'],
                    NFSHomeDirectory=[str(identity.ROOT / 'data')], GeneratedUID=[expected_uuid],
                    AuthenticationAuthority=user.get('AuthenticationAuthority'), IsHidden=None, RealName=None)
    differences = ', '.join(sorted(name for name in set(user) | set(expected) if user.get(name) != expected.get(name)))
    need(user == expected, 'Partial account differs from the inspected failure (' + differences + '); preserving it')
    need(users.get(identity.ACCOUNT) == number and list(users.values()).count(number) == 1,
         'Partial user identity collision')
    need(identity.ACCOUNT.casefold() not in {name.casefold() for name in groups}
         and number not in groups.values(), 'Preserving existing group or numeric collision')
    return dict(receipt, user_uuid=expected_uuid)


def finish_records(receipt):
    path = '/Users/' + identity.ACCOUNT
    # Preserve the OS-assigned GeneratedUID. Write the unusable marker first,
    # then explicitly enforce the final disabled authentication policy.
    for name, values in [('Password', ['*']), ('RealName', ['TimeSplitters Workshop Service']),
                         ('IsHidden', ['1']), ('AuthenticationAuthority', identity.DISABLED)]:
        identity.command('/usr/bin/dscl', '.', '-create', path, name, *values)
    identity.create_record('Groups', dict(PrimaryGroupID=[str(receipt['gid'])], Password=['*'],
                           RealName=['TimeSplitters Workshop Service'], GroupMembership=[identity.ACCOUNT]))
    receipt['group_uuid'] = identity.generated_uuid('Groups', receipt['group_uuid'])


def finalize(receipt, wire, expected_uuid, group_uuid):
    """Verify an explicitly identified completed repair, then publish only its receipt."""
    root, need = identity.ROOT, identity.require
    need({p.name for p in root.iterdir()} == {identity.MARKER, INTENT, 'data', 'secrets', 'releases'},
         'Unexpected partial repair contents; preserving them')
    path = root / INTENT
    info = path.lstat()
    need(stat.S_ISREG(info.st_mode) and info.st_uid == 0 and info.st_nlink == 1
         and stat.S_IMODE(info.st_mode) == 0o600 and 0 < info.st_size <= 8192,
         'Unknown repair intent')
    intent = json.loads(path.read_bytes())
    need(isinstance(intent, dict) and set(intent) == {'schema', 'producer', 'phase', 'original_receipt',
         'original_sha256', 'preserved_user_uuid'} and intent['schema'] == 1
         and intent['producer'] == 'tsfp-identity-repair-v1' and intent['phase'] == 'apply-started'
         and intent['original_receipt'] == receipt and receipt.get('phase') == 'preparing'
         and intent['original_sha256'] == hashlib.sha256(wire).hexdigest()
         and intent['preserved_user_uuid'] == expected_uuid, 'Changed or unrecognized repair intent')
    need(str(uuid.UUID(group_uuid)).upper() == group_uuid, 'Invalid final group UUID')
    result = dict(receipt, phase='complete', user_uuid=expected_uuid, group_uuid=group_uuid)
    identity.verify(result)
    # No account, group, directory or permission mutation occurs in this mode.
    temporary = root / '.identity-repaired.json'
    identity.write_new(temporary, result)
    os.replace(temporary, root / identity.MARKER)
    return result


def repair(expected_uuid, apply=False, finalize_group_uuid=None):
    need = identity.require
    need(sys.platform == 'darwin' and os.geteuid() == 0, 'Run inspection/repair locally with administrator sudo on the Mac')
    root = identity.ROOT
    identity.unlinked(root)
    info = root.lstat()
    need(stat.S_ISDIR(info.st_mode) and info.st_uid == 0 and stat.S_IMODE(info.st_mode) == 0o755,
         'Preserving changed service root')
    marker = root / identity.MARKER
    info = marker.lstat()
    need(stat.S_ISREG(info.st_mode) and info.st_nlink == 1 and info.st_uid == 0
         and stat.S_IMODE(info.st_mode) == 0o600 and 0 < info.st_size <= 8192,
         'Preserving unknown identity receipt')
    wire = marker.read_bytes()
    receipt = json.loads(wire)
    need(isinstance(receipt, dict), 'Invalid identity receipt')
    if receipt.get('phase') == 'complete':
        need(receipt.get('user_uuid') == expected_uuid, 'Completed identity differs from requested repair')
        return identity.existing()
    if finalize_group_uuid is not None:
        return finalize(receipt, wire, expected_uuid, finalize_group_uuid)
    need({path.name for path in root.iterdir()} == {identity.MARKER},
         'Partial repair or unknown service files exist; preserve for administrator inspection')
    user = identity.attributes('Users', *NAMES)
    result = assess(receipt, user, identity.inventory('Users', 'UniqueID'),
                    identity.inventory('Groups', 'PrimaryGroupID'), expected_uuid)
    need(not {0, 80}.intersection(os.getgrouplist(identity.ACCOUNT, result['uid'])),
         'Partial account has privileged membership')
    if not apply:
        return dict(result, phase='repair-plan-only', preserved_os_user_uuid=True)
    # This new root-owned intent preserves the original receipt before changes.
    # A failed attempt is never silently resumed or deleted by another invocation.
    identity.write_new(root / INTENT, dict(schema=1, producer='tsfp-identity-repair-v1', phase='apply-started',
                       original_receipt=receipt, original_sha256=hashlib.sha256(wire).hexdigest(),
                       preserved_user_uuid=expected_uuid))
    finish_records(result)
    for name, owner, group, mode in [('data', result['uid'], result['gid'], 0o700),
                                     ('secrets', result['uid'], result['gid'], 0o700), ('releases', 0, 0, 0o755)]:
        path = root / name
        path.mkdir(mode=mode)
        identity.command('/bin/chmod', '-N', str(path))
        os.chown(path, owner, group)
        os.chmod(path, mode)
    result['phase'] = 'complete'
    identity.verify(result)
    temporary = root / '.identity-repaired.json'
    identity.write_new(temporary, result)
    os.replace(temporary, marker)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expected-user-uuid', required=True)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--apply', action='store_true')
    modes.add_argument('--finalize-group-uuid', help='Verify an inspected completed repair and publish only its receipt')
    args = parser.parse_args()
    try:
        print(json.dumps(repair(args.expected_user_uuid, args.apply, args.finalize_group_uuid), indent=2))
        return 0
    except (identity.ProvisionError, OSError, ValueError, identity.subprocess.SubprocessError) as error:
        print('Service identity repair: ' + str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
