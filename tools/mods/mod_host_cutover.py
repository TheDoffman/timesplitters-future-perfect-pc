"""Read-only identity checks for the fixed Mac workshop cutover job set.

The caller supplies job specifications derived from verified immutable releases.
These observations do not authorize migration, stop jobs or issue admission.
"""
from pathlib import Path
import hashlib
import os
import plistlib
import re
import stat

from mod_host_migration import jobs
from mod_package import require
from mod_system_host import ROOT

LAUNCHD = Path('/Library/LaunchDaemons')
MAX_OUTPUT = 1024 * 1024
ROOT_UID = 0


def protected_directory(path):
    """Hold a nofollow root-owned directory through non-writable ancestors."""
    path = Path(os.path.abspath(path))
    fd = os.open(path.anchor, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        for part in path.parts[1:]:
            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            os.close(fd)
            fd = child
            info = os.fstat(fd)
            require(info.st_uid in (0, ROOT_UID) and not info.st_mode & 0o022,
                    'Unsafe cutover directory ancestor')
        require(os.fstat(fd).st_uid == ROOT_UID, 'Cutover directory must be root-owned')
        return fd
    except BaseException:
        os.close(fd)
        raise


def stamp(info):
    return (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns,
            info.st_uid, info.st_gid, stat.S_IMODE(info.st_mode), info.st_nlink)


def protected_bytes(path, mode, maximum):
    fd = protected_directory(path.parent)
    try:
        child = os.open(path.name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=fd)
        with os.fdopen(child, 'rb') as stream:
            before = os.fstat(stream.fileno())
            require(stat.S_ISREG(before.st_mode) and before.st_uid == ROOT_UID and before.st_nlink == 1
                    and stat.S_IMODE(before.st_mode) == mode and 0 < before.st_size <= maximum,
                    'Unsafe protected cutover file')
            data = stream.read(maximum+1)
            after = os.fstat(stream.fileno())
            require(len(data) == before.st_size and stamp(before) == stamp(after)
                    and stamp(os.stat(path.name, dir_fd=fd, follow_symlinks=False)) == stamp(after),
                    'Protected cutover file changed')
        current = protected_directory(path.parent)
        try:
            require((os.fstat(current).st_dev, os.fstat(current).st_ino)
                    == (os.fstat(fd).st_dev, os.fstat(fd).st_ino), 'Cutover directory was replaced')
        finally:
            os.close(current)
        return data, stamp(after)
    finally:
        os.close(fd)


def validate_entries(entries):
    require(isinstance(entries, list) and len(entries) == 10
            and all(isinstance(v, tuple) and len(v) == 2 and isinstance(v[0], Path)
                    and isinstance(v[1], dict) and isinstance(v[1].get('Label'), str) for v in entries)
            and sorted(spec['Label'] for _, spec in entries) == jobs(),
            'Use exactly the fixed cutover job set')
    for generated, spec in entries:
        role = spec['Label'].split('.')[3]
        if role == 'backup':
            require(generated.parent.parent == ROOT/'backup-jobs'
                    and re.fullmatch('[0-9a-f]{64}', generated.parent.name) and generated.name == 'worker.plist',
                    'Invalid protected backup job path')
        else:
            monitor = spec['Label'].endswith('.monitor')
            expected = ((ROOT/'monitor'/('monitor.plist' if monitor else 'scan.plist')) if role == 'scan'
                        else ROOT/'monitor'/'definitions.plist' if role == 'definitions'
                        else ROOT/'monitor'/role/('monitor.plist' if monitor else 'worker.plist'))
            require(generated == expected, 'Invalid protected host job path')


def job_files(entries, previous=None):
    """Attest all twenty staging/boot files against immutable factory bytes.

    Passing a previous result also requires unchanged file identities/metadata,
    so replacement with identical bytes cannot silently cross an observation.
    """
    validate_entries(entries)
    result = {}
    for generated, spec in entries:
        expected = plistlib.dumps(spec)
        require(0 < len(expected) <= 32768, 'Cutover job specification exceeds budget')
        for path, mode in ((generated, 0o444 if spec['Label'].endswith('.backup') else 0o600),
                           (LAUNCHD/(spec['Label']+'.plist'), 0o644)):
            data, identity = protected_bytes(path, mode, 32768)
            require(data == expected, 'Protected cutover job differs from immutable policy')
            result[str(path)] = dict(stamp=identity, sha256=hashlib.sha256(data).hexdigest())
    require(len(result) == 20 and (previous is None or result == previous),
            'Protected cutover jobs changed during observation')
    return result


def loaded_job(output, generated, specification):
    """Bind launchctl's top-level metadata, ignoring nested coalition fields."""
    label = specification['Label']
    arguments = specification['ProgramArguments']
    require(label in jobs() and isinstance(arguments, list) and 1 <= len(arguments) <= 16
            and all(isinstance(v, str) and v and not any(c in v for c in '\r\n\t\0') for v in arguments),
            'Invalid fixed cutover job specification')
    require(isinstance(output, str) and len(output.encode('utf-8')) <= MAX_OUTPUT
            and '\r' not in output and '\0' not in output
            and output.startswith('system/'+label+' = {\n') and output.endswith('\n}\n'),
            'Unrecognized loaded cutover job')

    def field(name, optional=False):
        found = re.findall(r'^\t'+re.escape(name)+r' = ([^\n]*)$', output, re.MULTILINE)
        require(len(found) == 1 or (optional and not found), 'Ambiguous loaded cutover job '+name)
        return found[0] if found else None

    path = field('path')
    require(path in (str(generated), str(LAUNCHD/(label+'.plist')))
            and field('type') == 'LaunchDaemon' and field('program') == arguments[0],
            'Loaded cutover job binding differs')
    blocks = re.findall(r'^\targuments = \{\n((?:\t\t[^\n]*\n)*)\t\}$', output, re.MULTILINE)
    require(len(blocks) == 1 and blocks[0].splitlines() == ['\t\t'+v for v in arguments],
            'Loaded cutover job arguments differ')
    # Count the opening field too: an extra malformed block must not be ignored.
    require(len(re.findall(r'^\targuments = ', output, re.MULTILINE)) == 1,
            'Ambiguous loaded cutover job arguments')
    pid = field('pid', optional=True)
    require(pid is None or (re.fullmatch(r'[0-9]+', pid) and 1 < int(pid) < 2**31),
            'Invalid loaded cutover job process')
    return dict(path=path, pid=None if pid is None else int(pid))


def process_rows(output):
    """Parse only PID/parent/process-group/UID, never command lines or env."""
    require(isinstance(output, str) and len(output.encode('utf-8')) <= MAX_OUTPUT,
            'Cutover process observation exceeds budget')
    rows = output.splitlines()
    require(0 < len(rows) <= 8192, 'Cutover process count exceeds budget')
    result = {}
    for row in rows:
        require(re.fullmatch(r'\s*[0-9]+\s+[0-9]+\s+[0-9]+\s+-?[0-9]+\s*', row),
                'Malformed cutover process observation')
        pid, parent, group, uid = map(int, row.split())
        require(0 < pid < 2**31 and 0 <= parent < 2**31 and 0 <= group < 2**31
                and -2 <= uid < 2**32 and pid not in result,
                'Ambiguous cutover process observation')
        result[pid] = (parent, group, uid)
    return result


def tracked_processes(roots, rows, uid):
    """Remember old descendants/groups; this is observation, never a PID kill list."""
    require(type(uid) is int and uid >= 600 and isinstance(roots, set)
            and len(roots) <= 10 and roots <= rows.keys(), 'Cutover job processes changed')
    tree = set(roots)
    while True:
        children = {pid for pid, (parent, _, _) in rows.items() if parent in tree} - tree
        if not children:
            break
        tree.update(children)
        require(len(tree) <= 128, 'Cutover descendant count exceeds budget')
    require(all(rows[pid][2] in (0, uid) for pid in tree), 'Cutover descendant owner differs')
    groups = {rows[pid][1] for pid in tree}
    require(0 not in groups, 'Unknown cutover process group')
    return dict(pids=sorted(tree), groups=sorted(groups))


def processes_gone(tracked, rows, uid):
    """Require old groups/trees AND every dedicated-identity process to be gone.

    A reused PID/group conservatively delays admission. No sampled PID is ever
    signalled. All fixed launchd labels must also be confirmed absent separately.
    """
    require(type(uid) is int and uid >= 600, 'Invalid cutover service identity')
    return (not set(tracked['pids']).intersection(rows)
            and not set(tracked['groups']).intersection(group for _, group, _ in rows.values())
            and all(owner != uid for _, _, owner in rows.values()))


def observe(entries, uid, command):
    """Observe every immutable job twice around a single numeric process sample.

    Both the generated protected path and installed boot-file path are valid
    launchd origins. The caller must verify their root-owned bytes against the
    immutable specification before calling; label matching alone is insufficient.
    """
    validate_entries(entries)

    def snapshot():
        result = {}
        for generated, spec in entries:
            code, output = command(['/bin/launchctl', 'print', 'system/'+spec['Label']])
            require(code == 0, 'Fixed cutover job is not loaded')
            result[spec['Label']] = loaded_job(output, generated, spec)
        return result

    before = snapshot()
    running = [v['pid'] for v in before.values() if v['pid'] is not None]
    require(len(running) == len(set(running)), 'Cutover jobs share a process identity')
    code, output = command(['/bin/ps', '-axo', 'pid=,ppid=,pgid=,uid='])
    require(code == 0, 'Cannot observe cutover process identities')
    rows = process_rows(output)
    tracked = tracked_processes(set(running), rows, uid)
    require(snapshot() == before, 'Cutover jobs changed during observation')
    return dict(jobs=before, tracked=tracked)


def preflight(entries, uid, command):
    """Attest root job files on both sides of the complete live observation."""
    files = job_files(entries)
    observation = observe(entries, uid, command)
    job_files(entries, files)
    return dict(files=files, observation=observation)
