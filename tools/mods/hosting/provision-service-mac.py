"""Plan or provision the locked workshop identity. Does not start any service."""
import argparse
import json
import os
from pathlib import Path
import plistlib
import stat
import subprocess
import sys
import uuid

ACCOUNT = '_tsfpworkshop'
ROOT = Path('/Library/Application Support/TSFP Mod Workshop')
MARKER = 'identity.json'
PRODUCER = 'tsfp-service-identity-v1'
DISABLED = [';DisabledUser;', ';DisabledTags;SecureToken']


class ProvisionError(ValueError):
    pass


def require(value, message):
    if not value:
        raise ProvisionError(message)


def command(*arguments):
    result = subprocess.run(list(arguments), stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=15,
                            env=dict(PATH='/usr/bin:/bin:/usr/sbin:/sbin', LC_ALL='C'))
    require(len(result.stdout) + len(result.stderr) <= 1024**2, 'Directory command output exceeded budget')
    action = Path(arguments[0]).name
    if action == 'dscl' and '-create' in arguments:
        index = arguments.index('-create')
        action += ' create ' + (arguments[index + 2] if len(arguments) > index + 2 else 'record')
    require(result.returncode == 0, 'Directory command failed (' + action + ', exit ' + str(result.returncode)
            + '): ' + result.stderr.decode('utf-8', 'replace')[:512]
            + '; preserve partial setup for administrator inspection')
    return result.stdout


def inventory(kind, attribute):
    return parse_inventory(command('/usr/bin/dscl', '.', '-list', '/' + kind, attribute).decode('utf-8'))


def parse_inventory(text):
    rows = text.splitlines()
    require(len(rows) <= 8192, 'Directory inventory exceeds budget')
    records = {}
    for row in rows:
        parts = row.rsplit(None, 1)
        require(len(parts) == 2 and parts[0] not in records, 'Malformed directory inventory')
        try:
            records[parts[0]] = int(parts[1])
        except ValueError as error:
            raise ProvisionError('Malformed directory numeric identity') from error
    return records


def plan(users, groups):
    require(ACCOUNT.casefold() not in {n.casefold() for n in (*users, *groups)},
            'Preserving an existing service account/group without our completed receipt')
    occupied = set(users.values()) | set(groups.values())
    number = next((n for n in range(600, 10000) if n not in occupied), None)
    require(number is not None, 'No unused service identity available')
    return dict(schema=1, producer=PRODUCER, account=ACCOUNT, uid=number, gid=number,
                root=str(ROOT), password_login=False, interactive_shell=False,
                administrator=False, services_started=False, public_endpoint_started=False)


def unlinked(path):
    require(all(not p.is_symlink() for p in (path, *path.parents)), 'Linked service installation path refused')


def write_new(path, value):
    wire = (json.dumps(value, sort_keys=True, indent=2) + '\n').encode()
    require(len(wire) <= 8192, 'Identity receipt exceeds budget')
    with os.fdopen(os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY | os.O_NOFOLLOW, 0o600), 'wb') as stream:
        stream.write(wire)
        stream.flush()
        os.fsync(stream.fileno())


def attributes(kind, *names):
    raw = command('/usr/bin/dscl', '-plist', '.', '-read', '/' + kind + '/' + ACCOUNT, *names)
    value = plistlib.loads(raw)
    require(isinstance(value, dict), 'Invalid service directory record')
    return {name: value.get('dsAttrTypeStandard:' + name) for name in names}


def verify(receipt):
    require(receipt.get('producer') == PRODUCER and receipt.get('schema') == 1
            and receipt.get('phase') == 'complete' and receipt.get('account') == ACCOUNT
            and receipt.get('root') == str(ROOT) and type(receipt.get('uid')) is int
            and 600 <= receipt['uid'] < 10000 and receipt.get('gid') == receipt['uid'],
            'Incomplete or unknown identity receipt; administrator inspection required')
    uid = receipt['uid']
    user = attributes('Users', 'UniqueID', 'PrimaryGroupID', 'UserShell', 'NFSHomeDirectory',
                      'AuthenticationAuthority', 'GeneratedUID', 'IsHidden')
    expected = dict(UniqueID=[str(uid)], PrimaryGroupID=[str(uid)], UserShell=['/usr/bin/false'],
                    NFSHomeDirectory=[str(ROOT / 'data')], AuthenticationAuthority=DISABLED,
                    GeneratedUID=[receipt['user_uuid']], IsHidden=['1'])
    require(user == expected, 'Service user attributes changed; preserving installation')
    group = attributes('Groups', 'PrimaryGroupID', 'GeneratedUID', 'GroupMembership')
    require(group == dict(PrimaryGroupID=[str(uid)], GeneratedUID=[receipt['group_uuid']], GroupMembership=[ACCOUNT]),
            'Service group attributes changed; preserving installation')
    users, groups = inventory('Users', 'UniqueID'), inventory('Groups', 'PrimaryGroupID')
    require(list(users.values()).count(uid) == 1 and list(groups.values()).count(uid) == 1
            and users.get(ACCOUNT) == uid and groups.get(ACCOUNT) == uid, 'Service numeric identity collision')
    require(80 not in os.getgrouplist(ACCOUNT, uid) and 0 not in os.getgrouplist(ACCOUNT, uid),
            'Workshop identity must not have administrator/root group membership')
    for path, owner, mode in [(ROOT, 0, 0o755), (ROOT / 'data', uid, 0o700),
                              (ROOT / 'secrets', uid, 0o700), (ROOT / 'releases', 0, 0o755)]:
        unlinked(path)
        info = path.stat()
        require(stat.S_ISDIR(info.st_mode) and info.st_uid == owner and stat.S_IMODE(info.st_mode) == mode,
                'Service directory ownership/mode changed')
    return receipt


def existing():
    unlinked(ROOT)
    require(ROOT.is_dir(), 'Preserving unknown service installation path')
    path = ROOT / MARKER
    info = path.lstat()
    require(stat.S_ISREG(info.st_mode) and info.st_nlink == 1 and info.st_uid == 0
            and stat.S_IMODE(info.st_mode) == 0o600 and 0 < info.st_size <= 8192,
            'Preserving unknown identity receipt')
    return verify(json.loads(path.read_bytes()))


def create_record(kind, values):
    path = '/' + kind + '/' + ACCOUNT
    command('/usr/bin/dscl', '.', '-create', path)
    for name, value in values.items():
        command('/usr/bin/dscl', '.', '-create', path, name, *value)


def generated_uuid(kind, fallback):
    value = attributes(kind, 'GeneratedUID')['GeneratedUID']
    if value is None:
        command('/usr/bin/dscl', '.', '-create', '/' + kind + '/' + ACCOUNT, 'GeneratedUID', fallback)
        return fallback
    require(len(value) == 1 and str(uuid.UUID(value[0])).upper() == value[0],
            'Unexpected OS-generated identity; preserve partial setup')
    return value[0]


def apply():
    require(sys.platform == 'darwin' and os.geteuid() == 0, 'Run --apply locally with administrator sudo on the Mac')
    unlinked(ROOT)
    if ROOT.exists():
        return existing()
    result = plan(inventory('Users', 'UniqueID'), inventory('Groups', 'PrimaryGroupID'))
    result.update(phase='preparing', user_uuid=str(uuid.uuid4()).upper(), group_uuid=str(uuid.uuid4()).upper())
    ROOT.mkdir(mode=0o755)  # New only. Never adopt, chown, delete or migrate unknown data.
    command('/bin/chmod', '-N', str(ROOT))
    os.chmod(ROOT, 0o755)
    write_new(ROOT / MARKER, result)
    # Authentication is disabled before assigning a usable numeric user ID.
    create_record('Users', dict(UserShell=['/usr/bin/false'], Password=['*'], AuthenticationAuthority=DISABLED,
                  UniqueID=[str(result['uid'])], PrimaryGroupID=[str(result['gid'])],
                  NFSHomeDirectory=[str(ROOT / 'data')],
                  RealName=['TimeSplitters Workshop Service'], IsHidden=['1']))
    result['user_uuid'] = generated_uuid('Users', result['user_uuid'])
    create_record('Groups', dict(PrimaryGroupID=[str(result['gid'])],
                  Password=['*'], RealName=['TimeSplitters Workshop Service'], GroupMembership=[ACCOUNT]))
    result['group_uuid'] = generated_uuid('Groups', result['group_uuid'])
    for name, owner, group, mode in [('data', result['uid'], result['gid'], 0o700),
                                     ('secrets', result['uid'], result['gid'], 0o700), ('releases', 0, 0, 0o755)]:
        path = ROOT / name
        path.mkdir(mode=mode)
        command('/bin/chmod', '-N', str(path))
        os.chown(path, owner, group)
        os.chmod(path, mode)
    result['phase'] = 'complete'
    verify(result)
    temporary = ROOT / '.identity-complete.json'
    write_new(temporary, result)
    os.replace(temporary, ROOT / MARKER)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply', action='store_true', help='Administrator-only account/directory creation; no services')
    args = parser.parse_args()
    try:
        require(sys.platform == 'darwin', 'Run identity provisioning on the Mac host')
        if args.apply:
            result = apply()
        else:
            unlinked(ROOT)
            require(not ROOT.exists(), 'Service installation already exists; use administrator --apply to verify its receipt')
            result = plan(inventory('Users', 'UniqueID'), inventory('Groups', 'PrimaryGroupID'))
            result['phase'] = 'plan-only'
        print(json.dumps(result, indent=2))
        return 0
    except (ProvisionError, OSError, ValueError, subprocess.SubprocessError) as error:
        print('Service identity: ' + str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
