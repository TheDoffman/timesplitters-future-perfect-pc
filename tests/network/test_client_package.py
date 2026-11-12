"""The companion includes player hosting without private community service sources."""
from pathlib import Path
import shutil
import subprocess
import sys

from bundle_online import COMPANION, repo_file


def test_standalone_client_imports_and_invitation_roundtrip(tmp_path):
    root = Path(__file__).resolve().parents[2]
    for name in COMPANION:
        shutil.copy2(repo_file(root, 'tools', name), tmp_path/name)
    check = '''
import sys
sys.path.insert(0, sys.argv[1])
import online, online_client, online_arcade_client, online_transport, online_private, online_upnp
from online_host import invitation, parse_invitation
value = ('https://example.test:41820', 'ab'*32, 'A'*16)
assert parse_invitation(invitation(*value)) == value
assert online_arcade_client.SCHEMA == 'tsfp-arcade-checkpoint-v1'
assert not {'lobby_server', 'online_migration', 'online_arcade_service', 'online_dashboard'} & set(sys.modules)
'''
    result = subprocess.run([sys.executable, '-I', '-c', check, str(tmp_path)],
                            capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, result.stdout + result.stderr
