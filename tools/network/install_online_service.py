"""Install the lobby/relay as a per-user macOS launch agent, without game data."""
import argparse
import hashlib
import ipaddress
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys

from online_host import certificate

LABEL = 'com.timesplitters.online'
SERVER_FILES = ('lobby_server.py', 'online_transport.py', 'online_client.py',
                'online_host.py', 'online_paths.py', 'online_migration.py', 'online_arcade_service.py',
                'online_dashboard.py', 'online_dashboard.html', 'online_dashboard.css', 'online_dashboard.js')


def prepare(root, address, *, python=None, directory_port=30820, relay_port=30821, lan_address=None, dashboard_port=None):
    """Prepare a reviewable installation; registration with launchd is separate."""
    root = Path(root).expanduser().resolve()
    ip = ipaddress.IPv4Address(address)
    if ip.is_unspecified or ip.is_multicast:
        raise ValueError('Use the IPv4 address players can reach')
    if not all(1 <= port <= 65535 for port in (directory_port, relay_port)):
        raise ValueError('Service ports must be between 1 and 65535')
    if dashboard_port is not None and (not 1 <= dashboard_port <= 65535 or dashboard_port == directory_port):
        raise ValueError('Dashboard needs a distinct TCP port from 1 to 65535')
    if lan_address:
        lan_ip = ipaddress.IPv4Address(lan_address)
        if not lan_ip.is_private or lan_ip.is_unspecified or lan_ip.is_multicast:
            raise ValueError('Use a private LAN IPv4 for the LAN override')
    source = Path(__file__).resolve().parent
    if root == source or source.is_relative_to(root):
        raise ValueError('Install into a separate service folder')
    marker = root/'installation.json'
    if root.exists() and any(root.iterdir()) and not marker.is_file():
        raise ValueError('Destination is not an existing TimeSplitters service or an empty folder')
    if marker.is_file() and json.loads(marker.read_text()).get('label') != LABEL:
        raise ValueError('Destination belongs to another service')
    if dashboard_port is None and (root/'service.plist').is_file():
        previous=plistlib.loads((root/'service.plist').read_bytes()).get('ProgramArguments',[])
        if '--dashboard-port' in previous:
            dashboard_port=int(previous[previous.index('--dashboard-port')+1])
            if not 1 <= dashboard_port <= 65535 or dashboard_port == directory_port:
                raise ValueError('Existing dashboard port is invalid')
    root.mkdir(parents=True, exist_ok=True)
    server = root/'server'
    server.mkdir(exist_ok=True)
    logs = root/'logs'
    logs.mkdir(exist_ok=True)
    hashes = {}
    for name in SERVER_FILES:
        target = server/name
        shutil.copy2(source/name, target)
        hashes[name] = hashlib.sha256(target.read_bytes()).hexdigest()
    cert, key, fingerprint = certificate(root/'certificate')
    if os.name == 'posix':
        key.chmod(0o600)
    executable = str(Path(python or sys.executable).resolve())
    plist = dict(Label=LABEL, ProgramArguments=[executable, '-I', '-B', str(server/'lobby_server.py'),
        '--bind', '127.0.0.1' if ip.is_loopback else '0.0.0.0', '--port', str(directory_port), '--cert', str(cert), '--key', str(key),
        '--relay-address', str(ip), '--relay-port', str(relay_port)],
        WorkingDirectory=str(server), RunAtLoad=True, KeepAlive=True, ThrottleInterval=10,
        StandardOutPath=str(logs/'server.out.log'), StandardErrorPath=str(logs/'server.err.log'),
        ProcessType='Background', Umask=0o077)
    # Isolated mode excludes the script directory. Add it explicitly through a
    # tiny stdlib entry rather than inheriting user PYTHONPATH/site customizations.
    plist['ProgramArguments'] = [executable, '-I', '-B', '-c',
        'import runpy,sys; sys.path.insert(0,sys.argv.pop(1)); '
        'runpy.run_module("lobby_server",run_name="__main__")', str(server), *plist['ProgramArguments'][4:]]
    if lan_address:
        plist['ProgramArguments'] += ['--relay-lan-address', str(lan_ip)]
    if dashboard_port is not None:
        plist['ProgramArguments'] += ['--dashboard-port', str(dashboard_port)]
    with (root/'service.plist').open('wb') as stream:
        plistlib.dump(plist, stream)
    (root/'client-service.json').write_text(json.dumps(dict(
        directory=f'https://{ip}:{directory_port}', fingerprint=fingerprint), indent=2)+'\n')
    (root/'installation.json').write_text(json.dumps(dict(label=LABEL, files=hashes), indent=2)+'\n')
    return root, plist


def register(root, plist):
    if sys.platform != 'darwin':
        raise ValueError('Launch agents can only be installed on macOS')
    agent = Path.home()/'Library/LaunchAgents'/f'{LABEL}.plist'
    domain = f'gui/{os.getuid()}'
    if agent.exists():
        old = plistlib.loads(agent.read_bytes())
        if old.get('WorkingDirectory') != plist['WorkingDirectory']:
            raise ValueError('An Online service is already installed from another folder')
        subprocess.run(['launchctl', 'bootout', domain, str(agent)], check=False, capture_output=True)
    agent.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root/'service.plist', agent)
    subprocess.run(['launchctl', 'bootstrap', domain, str(agent)], check=True)
    return agent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path.home()/'Library/Application Support/TimeSplitters Online')
    parser.add_argument('--address', required=True, help='Numeric IPv4 players use (LAN for tests, public for internet)')
    parser.add_argument('--directory-port', type=int, default=30820)
    parser.add_argument('--relay-port', type=int, default=30821)
    parser.add_argument('--lan-address', help='Optional private IPv4 for clients on the Mac mini LAN')
    parser.add_argument('--dashboard-port', type=int, help='Optional private dashboard TCP port; access remotely through SSH')
    parser.add_argument('--install', action='store_true', help='Register and start the per-user launch agent')
    args = parser.parse_args()
    root, plist = prepare(args.root, args.address, directory_port=args.directory_port,
                          relay_port=args.relay_port, lan_address=args.lan_address, dashboard_port=args.dashboard_port)
    if args.install:
        print(f'Started {register(root, plist)}')
    print(f'Service configuration: {root / "service.plist"}')
    print(f'Public client settings: {root / "client-service.json"}')


if __name__ == '__main__':
    main()
