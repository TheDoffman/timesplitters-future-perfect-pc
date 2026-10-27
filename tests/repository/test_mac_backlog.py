"""Keep independent feature handoffs addressable after branch integration."""


from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
STATUSES = {'Queued', 'In progress', 'Awaiting validation', 'Verified', 'Not applicable'}


def test_backlog_ids_and_queue_agree():
    text = (ROOT / 'docs/project/MAC_PORT_BACKLOG.md').read_text(encoding='utf-8')
    entries = re.findall(r'^### (MAC-\d+) [—-] ([^\n]+)\n(.*?)(?=^#{2,3} |\Z)',
                         text, re.M | re.S)
    assert entries, 'No feature entries found'
    assert len({entry[0] for entry in entries}) == len(entries), 'Duplicate feature ID'
    assert len({entry[1] for entry in entries}) == len(entries), 'Duplicate feature title'
    expected = {}
    for identifier, title, body in entries:
        status = re.search(r'^- \*\*Status:\*\* (.+)$', body, re.M)
        assert status and status[1] in STATUSES, f'{identifier}: missing or invalid status'
        expected[identifier] = (title, status[1])
    queue = text.split('## Queue\n', 1)[1].split('\n### ', 1)[0]
    rows = re.findall(r'^\| (MAC-\d+) \| ([^|]+) \| ([^|]+) \|$', queue, re.M)
    assert len({row[0] for row in rows}) == len(rows), 'Duplicate queue ID'
    assert {identifier: (title.strip(), status.strip())
            for identifier, title, status in rows} == expected
