"""Read-only identity checks for the fixed Mac workshop cutover job set.

The caller supplies job specifications derived from verified immutable releases.
These observations do not authorize migration, stop jobs or issue admission.
"""
from pathlib import Path
import re

from mod_host_migration import jobs
from mod_package import require

LAUNCHD = Path('/Library/LaunchDaemons')
MAX_OUTPUT = 1024 * 1024


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
    require(isinstance(entries, list) and len(entries) == 10
            and sorted(spec['Label'] for _, spec in entries) == jobs(),
            'Observe exactly the fixed cutover job set')

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
