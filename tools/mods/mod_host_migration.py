"""Journaled private configuration migration, after root-proven job quiescence.

This never controls launchd, installs a release, reads repository contents or
changes signing keys. The root cutover coordinator must supply fresh admission.
"""
from contextlib import contextmanager
import hashlib
import importlib.util
from itertools import islice
import json
import os
from pathlib import Path
import re
import stat
import time

from mod_package import read_json, require
from mod_system_host import ROOT

ROOT_UID = 0
PRODUCER = 'tsfp-private-generation-migration-v1'
FIELDS = ('scan/service.json', 'scan/definitions.json', 'scan/initialization.json',
          'host/gateway.json', 'host/publication.json', 'host/initialization.json')
GUARDS = ('host/api.json', 'host/trusted-root.json', 'host/catalog-1.pem')
SHA = re.compile(r'[0-9a-f]{64}\Z')
PHASES = {'prepared', 'applying', 'applied', 'rolling-back', 'rolled-back'}


def wire(value):
    return (json.dumps(value, sort_keys=True, separators=(',', ':'))+'\n').encode()


def digest(value):
    return hashlib.sha256(value).hexdigest()


def helpers():
    result = []
    for name in ('initialize-private-host-mac', 'initialize-private-scan-mac'):
        spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name('hosting')/(name+'.py'))
        module = importlib.util.module_from_spec(spec);spec.loader.exec_module(module);result.append(module)
    return result


def releases(before, after):
    before, after = Path(before), Path(after)
    require(before.parent == after.parent == ROOT/'releases' and SHA.fullmatch(before.name)
            and SHA.fullmatch(after.name) and before != after, 'Invalid migration generation pair')
    return before, after


def expected(before, after, root, key, origin):
    before, after = releases(before, after)
    host, scan = helpers()
    pin = digest(root);host.signing(root, key, origin, pin)
    generations = []
    for generation in (before, after):
        payloads = {name: wire(value) for name, value in host.configs(generation, origin).items()}
        payloads.update({'trusted-root.json': root, 'catalog-1.pem': key})
        values = {'host/'+name: value for name, value in payloads.items()}
        values['host/initialization.json'] = wire(dict(host.marker(generation, origin, pin, payloads), phase='complete'))
        configs, marker = scan.plan(generation)
        values.update({'scan/'+name: wire(value) for name, value in configs.items()})
        values['scan/initialization.json'] = wire(dict(marker, phase='complete'))
        generations.append(values)
    old, new = generations
    require(set(old) == set(new) == {*FIELDS, *GUARDS}
            and all(old[name] == new[name] for name in GUARDS)
            and all(old[name] != new[name] for name in FIELDS), 'Unsupported generation configuration contract')
    manifest = dict(schema=1, producer=PRODUCER, before=before.name, after=after.name, origin=origin,
                    guards={name: digest(old[name]) for name in GUARDS},
                    files={name: dict(before=digest(old[name]), after=digest(new[name])) for name in FIELDS})
    # Returned payloads contain only configuration/marker bytes, never key material.
    return dict(manifest=manifest, sha256=digest(wire(manifest)),
                before={name: old[name] for name in FIELDS}, after={name: new[name] for name in FIELDS})


def _directory(path, *, private=False, root_owned=False):
    path = Path(os.path.abspath(path));fd = os.open(path.anchor, os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    try:
        for part in path.parts[1:]:
            child = os.open(part, os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW, dir_fd=fd);os.close(fd);fd = child
            info = os.fstat(fd)
            require(info.st_uid in (0, ROOT_UID, os.geteuid()) and not info.st_mode & 0o022, 'Unsafe migration ancestor')
        info = os.fstat(fd)
        require(not root_owned or info.st_uid == ROOT_UID, 'Migration admission directory must be root-owned')
        require(not private or (info.st_uid == os.geteuid() and stat.S_IMODE(info.st_mode) == 0o700), 'Unsafe private migration directory')
        return fd
    except BaseException:
        os.close(fd);raise


def _stamp(info):
    return info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns


def _read(fd, name, limit=8192, *, owner=None, mode=0o600):
    require('/' not in name and name not in ('', '.', '..'), 'Invalid fixed migration member')
    child = os.open(name, os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK, dir_fd=fd)
    with os.fdopen(child, 'rb') as stream:
        before = os.fstat(stream.fileno());uid = os.geteuid() if owner is None else owner
        require(stat.S_ISREG(before.st_mode) and before.st_uid == uid and before.st_nlink == 1
                and stat.S_IMODE(before.st_mode) == mode and 0 < before.st_size <= limit, 'Unsafe migration member')
        value = stream.read(limit+1);after = os.fstat(stream.fileno())
        require(len(value) == before.st_size and _stamp(before) == _stamp(after), 'Migration member changed while reading')
    return value, _stamp(after)


def _new(fd, name, value):
    require(isinstance(name, str) and '/' not in name and name not in ('', '.', '..')
            and isinstance(value, bytes) and 0 < len(value) <= 8192, 'Invalid fixed migration output')
    child = os.open(name, os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW, 0o600, dir_fd=fd)
    with os.fdopen(child, 'wb') as output:
        os.fchmod(output.fileno(), 0o600);output.write(value);output.flush();os.fsync(output.fileno())
    os.fsync(fd)


@contextmanager
def tree():
    data = _directory(ROOT/'data', private=True);opened = []
    try:
        for name in ('scan', 'host'):
            fd = os.open(name, os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW, dir_fd=data);opened.append(fd)
            info = os.fstat(fd)
            require(info.st_uid == os.geteuid() and stat.S_IMODE(info.st_mode) == 0o700, 'Unsafe private generation directory')
        yield data, dict(zip(('scan', 'host'), opened))
    finally:
        for fd in opened:os.close(fd)
        os.close(data)


def _visible(data, folders):
    a, b = os.stat(ROOT/'data', follow_symlinks=False), os.fstat(data)
    require(stat.S_ISDIR(a.st_mode) and (a.st_dev,a.st_ino) == (b.st_dev,b.st_ino)
            and b.st_uid == os.geteuid() and stat.S_IMODE(b.st_mode) == 0o700, 'Private data directory was replaced')
    for name, fd in folders.items():
        a, b = os.stat(name, dir_fd=data, follow_symlinks=False), os.fstat(fd)
        require(stat.S_ISDIR(a.st_mode) and (a.st_dev,a.st_ino) == (b.st_dev,b.st_ino)
                and b.st_uid == os.geteuid() and stat.S_IMODE(b.st_mode) == 0o700, 'Generation directory was replaced')


def _layout(folders, nonce=None):
    allowed = dict(scan={'service.json','definitions.json','initialization.json','repository','database','worker-state','definitions-state'},
                   host={'api.json','publication.json','gateway.json','initialization.json','trusted-root.json','catalog-1.pem',
                         'api-state','publication-state','gateway-state','root-updates'})
    for name, fd in folders.items():
        with os.scandir(fd) as directory:entries = {entry.name for entry in islice(directory, len(allowed[name])+len(FIELDS)+1)}
        extras = entries-allowed[name]
        require(allowed[name].issubset(entries) and len(entries) <= len(allowed[name])+len(FIELDS), 'Private generation layout differs')
        temporary_names = {'.migration-'+str(nonce)+'-'+str(i) for i, path in enumerate(FIELDS) if path.startswith(name+'/')}
        require(not extras or (nonce is not None and extras.issubset(temporary_names)), 'Unknown private generation members; preserving them')
        directories = {'repository','database','worker-state','definitions-state'} if name=='scan' else {'api-state','publication-state','gateway-state','root-updates'}
        for member in directories:
            child = os.open(member, os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW, dir_fd=fd)
            try:
                info = os.fstat(child)
                require(info.st_uid == os.geteuid() and stat.S_IMODE(info.st_mode) == 0o700, 'Unsafe generation data directory')
            finally:os.close(child)
    updates = os.open('root-updates', os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW, dir_fd=folders['host'])
    try:
        with os.scandir(updates) as directory:require(next(directory, None) is None, 'Root rotation needs an explicit generation migration')
    finally:os.close(updates)


def _plan(folders, before, after):
    root, _ = _read(folders['host'], 'trusted-root.json', 65536)
    key, _ = _read(folders['host'], 'catalog-1.pem', 16384)
    marker, _ = _read(folders['host'], 'initialization.json')
    value = read_json(marker);require(isinstance(value, dict), 'Invalid current generation marker')
    return expected(before, after, root, key, value.get('origin'))


def _current(folders, plan):
    result = {}
    for path in (*FIELDS, *GUARDS):
        folder, name = path.split('/');limit = 65536 if name=='trusted-root.json' else 16384 if name=='catalog-1.pem' else 8192
        value, stamp = _read(folders[folder], name, limit)
        if path in GUARDS:require(digest(value) == plan['manifest']['guards'][path], 'Unchanged generation input differs')
        else:
            require(value in (plan['before'][path], plan['after'][path]), 'Unknown generation bytes; preserving them')
            result[path] = value, stamp
    return result


def _profiles(plan, before, after):
    for generation in (before, after):
        fd = _directory(generation/'source/config/mods', root_owned=True)
        try:value, _ = _read(fd, 'public-workshop.json', owner=ROOT_UID, mode=0o644)
        finally:os.close(fd)
        profile = read_json(value)
        require(isinstance(profile, dict) and set(profile) == {'schema','origin','trusted_root_sha256','status'}
                and type(profile['schema']) is int and profile['schema'] == 1
                and profile['status'] in ('active','pending-public-acceptance')
                and profile['origin'] == plan['manifest']['origin']
                and profile['trusted_root_sha256'] == plan['manifest']['guards']['host/trusted-root.json'],
                'Immutable generation origin/root binding differs')


def inspect(before, after):
    before, after = releases(before, after)
    with tree() as (data, folders):
        _layout(folders);plan = _plan(folders, before, after);_profiles(plan, before, after);current = _current(folders, plan)
        require(all(value == plan['before'][path] for path, (value, _) in current.items()), 'Current generation is not the expected base')
        _visible(data, folders)
        return dict(manifest=plan['manifest'], sha256=plan['sha256'], files=len(FIELDS), services_changed=False)


def jobs():
    names = []
    for role, config in [('scan', ROOT/'data/scan/service.json'), *((role, ROOT/'data/host'/(role+'.json')) for role in ('api','publication','gateway'))]:
        label = 'org.timesplitters.workshop.'+role+'.'+digest(str(config).encode())[:24]
        names.extend((label, label+'.monitor'))
    names.extend(('org.timesplitters.workshop.definitions.'+digest(str(ROOT/'data/scan/definitions.json').encode())[:24], 'org.timesplitters.workshop.backup'))
    return sorted(names)


def _admission(plan, nonce, clock):
    fd = _directory(ROOT/'monitor', root_owned=True)
    try:value, _ = _read(fd, 'upgrade-quiescence.json', owner=ROOT_UID, mode=0o644)
    finally:os.close(fd)
    data = read_json(value)
    require(isinstance(data, dict) and set(data) == {'schema','producer','before','after','uid','nonce','plan_sha256','backup_sha256','time','stopped_jobs','quiesced_at','backup_verified_at'}
            and type(data['schema']) is int and data['schema'] == 1 and data['producer'] == 'tsfp-upgrade-quiescence-v1'
            and data['before'] == plan['manifest']['before'] and data['after'] == plan['manifest']['after']
            and type(data['uid']) is int and data['uid'] == os.geteuid() and data['nonce'] == nonce
            and data['plan_sha256'] == plan['sha256'] and isinstance(data['backup_sha256'], str) and SHA.fullmatch(data['backup_sha256'])
            and type(data['time']) is int and type(data['quiesced_at']) is int and type(data['backup_verified_at']) is int
            and 0 <= data['quiesced_at'] <= data['backup_verified_at'] <= data['time']
            and 0 <= clock()-data['time'] <= 30 and 0 <= clock()-data['backup_verified_at'] <= 300
            and data['stopped_jobs'] == jobs(),
            'Fresh root-proven backup/job quiescence is required')
    return value


@contextmanager
def _journal(data, nonce, plan, allow_create):
    import fcntl
    try:os.mkdir('host-migrations', mode=0o700, dir_fd=data);os.fsync(data)
    except FileExistsError:pass
    root = os.open('host-migrations', os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW, dir_fd=data)
    lease = None;folder = None
    try:
        info = os.fstat(root);require(info.st_uid == os.geteuid() and stat.S_IMODE(info.st_mode) == 0o700, 'Unsafe migration journal root')
        try:_new(root, 'lease', b'TSFPMIG1\n')
        except FileExistsError:pass
        lease = os.open('lease', os.O_RDWR|os.O_NOFOLLOW|os.O_NONBLOCK, dir_fd=root)
        _, lease_stamp = _read(root, 'lease');require(os.read(lease, 16) == b'TSFPMIG1\n' and _stamp(os.fstat(lease)) == lease_stamp, 'Invalid migration lease')
        fcntl.flock(lease, fcntl.LOCK_EX|fcntl.LOCK_NB)
        require(_stamp(os.stat('lease', dir_fd=root, follow_symlinks=False)) == lease_stamp, 'Migration lease was replaced')
        with os.scandir(root) as directory:entries = {entry.name for entry in islice(directory, 34)}
        require(len(entries) <= 33 and all(name=='lease' or SHA.fullmatch(name) for name in entries), 'Migration journal capacity/unknown entries; preserving them')
        fresh = False
        try:folder = os.open(nonce, os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW, dir_fd=root)
        except FileNotFoundError:
            require(allow_create and len(entries) < 33, 'Missing recovery journal or migration capacity reached')
            os.mkdir(nonce, mode=0o700, dir_fd=root);os.fsync(root);fresh = True
            folder = os.open(nonce, os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW, dir_fd=root)
        info = os.fstat(folder);require(info.st_uid == os.geteuid() and stat.S_IMODE(info.st_mode) == 0o700, 'Unsafe migration journal')
        if fresh:
            for i, path in enumerate(FIELDS):
                for side in ('before','after'):_new(folder, str(i)+'.'+side, plan[side][path])
            _new(folder, 'manifest.json', wire(plan['manifest']))
            _new(folder, 'state.json', wire(dict(schema=1, producer=PRODUCER, plan_sha256=plan['sha256'], phase='prepared')))
        expected_names = {'manifest.json','state.json', *(str(i)+'.'+side for i in range(len(FIELDS)) for side in ('before','after'))}
        with os.scandir(folder) as directory:names = {entry.name for entry in islice(directory, len(expected_names)+2)}
        require(names in (expected_names, expected_names|{'.state-next'}), 'Incomplete/unknown migration journal; preserving it')
        require(_read(folder, 'manifest.json')[0] == wire(plan['manifest']), 'Migration journal generation changed')
        for i, path in enumerate(FIELDS):
            for side in ('before','after'):require(_read(folder, str(i)+'.'+side)[0] == plan[side][path], 'Migration journal payload changed')
        yield folder, fresh
    finally:
        if folder is not None:os.close(folder)
        if lease is not None:os.close(lease)
        os.close(root)


def _state(fd, plan, phase):
    current, stamp = _read(fd, 'state.json');value = read_json(current)
    def valid(value):return (isinstance(value, dict) and set(value) == {'schema','producer','plan_sha256','phase'}
            and type(value['schema']) is int and value['schema'] == 1 and value['producer'] == PRODUCER
            and value['plan_sha256'] == plan['sha256'] and isinstance(value['phase'], str) and value['phase'] in PHASES)
    require(valid(value), 'Unknown migration state; preserving it')
    try:pending, pending_stamp = _read(fd, '.state-next')
    except FileNotFoundError:pass
    else:
        require(valid(read_json(pending)) and _stamp(os.stat('.state-next', dir_fd=fd, follow_symlinks=False)) == pending_stamp, 'Unknown pending migration state')
        os.unlink('.state-next', dir_fd=fd);os.fsync(fd)
    target = wire(dict(value, phase=phase))
    _replace(fd, 'state.json', current, stamp, target, '.state-next')


def _replace(fd, name, current, stamp, target, temporary):
    try:_new(fd, temporary, target)
    except FileExistsError:require(_read(fd, temporary)[0] == target, 'Unknown migration temporary; preserving it')
    now, now_stamp = _read(fd, name)
    require(now == current and now_stamp == stamp, 'Migration destination changed; preserving it')
    require(_stamp(os.stat(name, dir_fd=fd, follow_symlinks=False)) == stamp, 'Migration destination was replaced')
    os.replace(temporary, name, src_dir_fd=fd, dst_dir_fd=fd);os.fsync(fd)


def migrate(before, after, nonce, *, rollback=False, clock=time.time):
    require(isinstance(nonce, str) and SHA.fullmatch(nonce) and type(rollback) is bool, 'Invalid migration operation')
    before, after = releases(before, after)
    with tree() as (data, folders):
        _layout(folders, nonce);plan = _plan(folders, before, after);_profiles(plan, before, after)
        proof = _admission(plan, nonce, clock);initial = _current(folders, plan)
        allow_create = not rollback and all(value == plan['before'][path] for path, (value, _) in initial.items())
        with _journal(data, nonce, plan, allow_create) as (journal, fresh):
            initial = _current(folders, plan)
            require(not fresh or all(value == plan['before'][path] for path, (value, _) in initial.items()), 'Base generation changed before journal admission')
            side, phase = ('before','rolling-back') if rollback else ('after','applying')
            _state(journal, plan, phase)
            for i, path in enumerate(FIELDS):
                require(_admission(plan, nonce, clock) == proof, 'Migration admission changed')
                _visible(data, folders);current = _current(folders, plan);value, stamp = current[path]
                folder, name = path.split('/');temporary = '.migration-'+nonce+'-'+str(i)
                target = plan[side][path]
                try:pending, pending_stamp = _read(folders[folder], temporary)
                except FileNotFoundError:pass
                else:
                    require(pending in (plan['before'][path],plan['after'][path]) and _stamp(os.stat(temporary, dir_fd=folders[folder], follow_symlinks=False)) == pending_stamp, 'Unknown migration temporary')
                    if pending != target:os.unlink(temporary, dir_fd=folders[folder]);os.fsync(folders[folder])
                if value != target:_replace(folders[folder], name, value, stamp, target, temporary)
                elif temporary in os.listdir(folders[folder]):
                    require(_read(folders[folder], temporary)[0] in (plan['before'][path],plan['after'][path]), 'Unknown migration temporary')
                    os.unlink(temporary, dir_fd=folders[folder]);os.fsync(folders[folder])
            require(all(value == plan[side][path] for path, (value, _) in _current(folders, plan).items()), 'Migration did not reach its exact generation')
            _state(journal, plan, 'rolled-back' if rollback else 'applied')
            _visible(data, folders);require(_admission(plan, nonce, clock) == proof, 'Migration admission changed at completion')
            return dict(schema=1, phase='rolled-back' if rollback else 'applied', plan_sha256=plan['sha256'],
                        before=before.name, after=after.name, files=len(FIELDS), services_changed=False,
                        repository_modified=False, keys_modified=False)
