"""Bounded numeric process sampling for owned Mac launchd services."""
import re
import subprocess
from mod_package import require


def run_command(arguments):
    result = subprocess.run(arguments, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=3, check=False)
    # These fixed OS utilities read only this owned job, never arbitrary uploaded
    # command output or a full user-domain environment dump.
    require(len(result.stdout) <= 1024 * 1024, 'Host utility output exceeded its budget')
    return result.returncode, result.stdout.decode('utf-8', errors='strict')


def managed_pid(output):
    values = re.findall(r'^\s*pid = ([0-9]+)\s*$', output, re.MULTILINE)
    require(len(values) <= 1, 'Ambiguous managed worker identity')
    return int(values[0]) if values else None


def sample(pid, command=run_command, uid=None, max_processes=8):
    # Numeric process metadata only: no argv/environment or personal file names.
    # Session/group changes do not alter this parent-child relationship.
    code, output = command(['/bin/ps', '-axo', 'pid=,ppid=,rss=,uid='])
    require(code == 0, 'Cannot sample managed scan descendants')
    rows = output.splitlines()
    require(len(rows) <= 8192, 'Host process metadata budget exceeded')
    processes, children = {}, {}
    for row in rows:
        # Darwin prints the unrelated system "nobody" UID as -2. Only the
        # managed subtree must match the configured nonnegative service UID.
        require(re.fullmatch(r'\s*[0-9]+\s+[0-9]+\s+[0-9]+\s+-?[0-9]+\s*', row), 'Malformed process sample')
        identity, parent, rss, owner = map(int, row.split())
        require(identity not in processes, 'Duplicate process sample')
        processes[identity] = (rss, owner)
        children.setdefault(parent, []).append(identity)
    if pid not in processes:
        return None
    expected_uid = processes[pid][1] if uid is None else uid
    pending, seen, total = [pid], set(), 0
    while pending:
        current = pending.pop()
        require(current not in seen and len(seen) < max_processes, 'Managed scan process budget exceeded')
        seen.add(current)
        rss, owner = processes[current]
        require(owner == expected_uid, 'Managed scan owner changed')
        total += rss * 1024
        pending.extend(children.get(current, ()))
    return total
