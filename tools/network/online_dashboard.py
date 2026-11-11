"""Private, read-only service dashboard. No game credentials enter snapshots."""
from collections import Counter, deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import ipaddress
import json
import os
from pathlib import Path
import platform
import socket
import threading
import time
from urllib.parse import urlsplit

ASSETS = {'/': ('online_dashboard.html', 'text/html; charset=utf-8'),
          '/dashboard.css': ('online_dashboard.css', 'text/css; charset=utf-8'),
          '/dashboard.js': ('online_dashboard.js', 'text/javascript; charset=utf-8')}


def age(now, timestamp):
    return round(max(0., now-timestamp), 2) if timestamp is not None else None


def route(member, now):
    endpoint = member.get('endpoint')
    return dict(registered=bool(endpoint and now-member.get('seen', 0) <= 5),
                route=('LAN' if ipaddress.ip_address(endpoint[0]).is_private else 'Public') if endpoint else 'Unregistered',
                relay_age=age(now, member['seen']) if endpoint else None)


class Monitor:
    def __init__(self, directory, relay=None, *, directory_port=None, clock=time.monotonic):
        self.directory, self.relay, self.directory_port = directory, relay, directory_port
        self.clock = clock
        self.started = clock()
        self.sampled_at = self.started
        self.lock = threading.Lock()
        self.requests = Counter()
        self.failures = Counter()
        self.durations = deque(maxlen=256)
        self.events = deque(maxlen=100)
        self.samples = deque(maxlen=180)
        self.workers = 0
        self.previous = None
        self.rooms = {}
        self.state = {}
        self.dashboard_port = None
        self.event_id = 0

    def event(self, kind, message, level='info'):
        # Caller holds this monitor's lock. Never record request paths/bodies.
        self.event_id += 1
        self.events.append(dict(id=self.event_id, at=time.time(), kind=kind, message=message, level=level))

    def http(self, method, status, seconds):
        with self.lock:
            self.requests[str(status)] += 1
            self.durations.append(max(0., seconds)*1000)
            if status >= 400:
                self.event('http', f'{method if method in ("GET","POST","PUT","DELETE") else "Request"} returned HTTP {status}', 'warning')

    def failure(self, kind):
        with self.lock:
            self.failures[kind] += 1
            self.event('connection', {'timeout': 'Connection or TLS handshake timed out',
                'tls': 'TLS connection failed', 'disconnect': 'Client connection closed early',
                'internal': 'Internal request handler error'}.get(kind, 'Connection failed'),
                'info' if kind == 'disconnect' else 'warning')

    def worker(self, change):
        with self.lock:
            self.workers = max(0, self.workers+change)

    def room_snapshot(self):
        d = self.directory
        with d.lock:
            now = d.clock()
            rooms = []
            for room in d.arcade.rooms.values():
                if room['closed'] or (not room['migration'] and
                        room['members'].get(room['host_peer'], {}).get('expires', 0) <= now):
                    continue
                migration = room['migration']
                checkpoint = (migration.selected or (migration.checkpoints[-1] if migration.checkpoints else None)) if migration else None
                peers = []
                for peer, member in room['members'].items():
                    tracked = migration.members.get(peer) if migration else None
                    peers.append(dict(id=peer, native_id=member['native_peer'], active=member['active'],
                        host=peer == room['host_peer'], candidate=bool(migration and peer == migration.candidate),
                        progress=member['native_counter'], progress_age=age(now, member['native_at']),
                        lease_remaining=round(max(0, migration.lease_seconds-(now-tracked.seen)), 2) if tracked else None,
                        backup_ack=bool(checkpoint and tracked and checkpoint.digest in tracked.acknowledged),
                        **route(member, now)))
                backup = dict(sequence=checkpoint.sequence, age=age(now, checkpoint.created),
                    acknowledgements=sum(peer['backup_ack'] for peer in peers),
                    bots=checkpoint.payload[50] if len(checkpoint.payload) >= 208 else None) if checkpoint else None
                rooms.append(dict(id='v3-'+room['code'], name=room['name'], protocol='Arcade v3',
                    mode='Arcade', phase=migration.phase if migration else 'lobby',
                    epoch=room['epoch'], host=room['host_peer'], backup=backup, peers=peers,
                    players=len(peers), capacity=16, migration_supported=True))
            for room in d.online.values():
                if room['expires'] <= now:
                    continue
                members={1:room['relay_host'], **room['peers']}
                peers=[dict(id=peer, host=peer == 1, native_id=None, active=None, progress=None,
                    progress_age=None, lease_remaining=None, backup_ack=False, candidate=False,
                    **route(member, now)) for peer, member in members.items()
                    if member.get('expires', now+1) > now]
                rooms.append(dict(id='v2-'+room['code'], name=room['name'], protocol='Legacy v2',
                    mode=room['mode'].title(), phase='legacy', epoch=None, host=1, backup=None,
                    peers=peers, players=len(peers), capacity=4 if room['mode']=='story' else 16,
                    migration_supported=False))
            for code, room in d.lobbies.items():
                if room['expires'] > now:
                    rooms.append(dict(id='v1-'+code, name=room['name'], protocol='Directory v1',
                        mode='Legacy', phase='legacy', epoch=None, host=None, backup=None, peers=[],
                        players=room['players'], capacity=room['capacity'], migration_supported=False))
        return rooms

    def sample(self):
        now = self.clock()
        rooms = self.room_snapshot()  # Read only: never poll, expire or renew a lease.
        relay = self.relay
        counters = dict(forwarded=relay.forwarded if relay else 0, dropped=relay.dropped if relay else 0,
            received_bytes=relay.received_bytes if relay else 0, sent_bytes=relay.sent_bytes if relay else 0)
        cpu = time.process_time()
        previous = self.previous
        dt = max(.001, now-previous[0]) if previous else 1
        rates = {key:round(max(0, value-previous[1][key])/dt, 2) if previous else 0 for key, value in counters.items()}
        cpu_percent = round(max(0, (cpu-previous[2])/dt)*100/max(1, os.cpu_count() or 1), 1) if previous else 0
        self.previous = now, counters, cpu
        try:
            import resource
            peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
            peak *= 1 if platform.system() == 'Darwin' else 1024
        except ImportError:
            peak = None
        with self.lock:
            current = {room['id']:room for room in rooms}
            for key, room in current.items():
                before = self.rooms.get(key)
                if before is None:
                    self.event('lobby', f'{room["name"]}: lobby connected')
                else:
                    if before['host'] != room['host']:
                        self.event('host', f'{room["name"]}: host changed from player {before["host"] or "pending"} to {room["host"] or "pending"}')
                    if before['phase'] != room['phase']:
                        self.event('phase', f'{room["name"]}: {room["phase"]}')
                    if before['players'] != room['players']:
                        self.event('players', f'{room["name"]}: {room["players"]} players connected')
            for key, room in self.rooms.items():
                if key not in current:self.event('lobby', f'{room["name"]}: lobby closed')
            self.rooms = current
            sorted_times=sorted(self.durations)
            http_errors=sum(value for key,value in self.requests.items() if int(key)>=400)
            self.samples.append(dict(at=time.time(), packets=rates['forwarded'], dropped=rates['dropped'],
                rx=rates['received_bytes'], tx=rates['sent_bytes'], cpu=cpu_percent))
            self.state = dict(at=time.time(), uptime=round(now-self.started), hostname=socket.gethostname(),
                platform=platform.system(), cpu_percent=cpu_percent, peak_memory=peak,
                directory_port=self.directory_port, relay_port=relay.socket.getsockname()[1] if relay else None,
                dashboard_port=self.dashboard_port,
                relay_enabled=relay is not None, counters=counters, rates=rates,
                rooms=rooms, players=sum(room['players'] for room in rooms),
                http=dict(requests=sum(self.requests.values()), errors=http_errors, statuses=dict(self.requests),
                    failures=dict(self.failures), workers=self.workers, worker_limit=32,
                    p95_ms=round(sorted_times[min(len(sorted_times)-1, int(len(sorted_times)*.95))], 1) if sorted_times else 0))
            self.sampled_at = now

    def snapshot(self):
        with self.lock:
            # Plain whitelist data only; serialization never visits service objects.
            return {**self.state, 'sample_age':age(self.clock(),self.sampled_at),
                    'events':list(reversed(self.events)), 'samples':list(self.samples)}


class DashboardHandler(BaseHTTPRequestHandler):
    def setup(self):
        super().setup()
        self.connection.settimeout(3)

    def log_message(self, *args):pass

    def do_GET(self):
        allowed={f'127.0.0.1:{self.server.server_port}', f'localhost:{self.server.server_port}'}
        if self.headers.get('Host') not in allowed or self.headers.get('Sec-Fetch-Site') == 'cross-site':
            self.send_error(403);return
        path=urlsplit(self.path).path
        if path == '/api/status':
            data=json.dumps(self.server.monitor.snapshot(), allow_nan=False).encode()
            mime='application/json'
        elif path in ASSETS:
            name,mime=ASSETS[path]
            data=(Path(__file__).resolve().parent/name).read_bytes()
        else:
            self.send_error(404);return
        self.send_response(200)
        self.send_header('Content-Type',mime)
        self.send_header('Content-Length',str(len(data)))
        self.send_header('Cache-Control','no-store')
        self.send_header('X-Content-Type-Options','nosniff')
        self.send_header('X-Frame-Options','DENY')
        self.send_header('Referrer-Policy','no-referrer')
        self.send_header('Content-Security-Policy',"default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self'; img-src 'self'; frame-ancestors 'none'; base-uri 'none'")
        self.end_headers();self.wfile.write(data)


class DashboardServer(ThreadingHTTPServer):
    daemon_threads=True
    allow_reuse_address=not hasattr(socket,'SO_EXCLUSIVEADDRUSE')

    def server_bind(self):
        if hasattr(socket,'SO_EXCLUSIVEADDRUSE'):
            self.socket.setsockopt(socket.SOL_SOCKET,socket.SO_EXCLUSIVEADDRUSE,1)
        super().server_bind()

    def __init__(self, port, monitor):
        super().__init__(('127.0.0.1',port),DashboardHandler)
        self.monitor=monitor
        self.slots=threading.BoundedSemaphore(8)

    def process_request(self,request,client_address):
        if not self.slots.acquire(blocking=False):
            self.shutdown_request(request);return
        try:super().process_request(request,client_address)
        except BaseException:
            self.slots.release();raise

    def process_request_thread(self,request,client_address):
        try:
            try:self.finish_request(request,client_address)
            except (ConnectionError,TimeoutError):pass
            finally:self.shutdown_request(request)
        finally:self.slots.release()


class Dashboard:
    def __init__(self, directory, relay=None, *, port=41822, directory_port=None):
        self.monitor=Monitor(directory,relay,directory_port=directory_port)
        self.server=DashboardServer(port,self.monitor)
        self.monitor.dashboard_port=self.server.server_port
        self.stopped=threading.Event()
        self.threads=[]

    def start(self):
        self.monitor.sample()
        def sample_loop():
            while not self.stopped.wait(1):
                try:self.monitor.sample()
                except Exception:self.monitor.failure('internal')
        self.threads=[threading.Thread(target=target,daemon=True) for target in (sample_loop,self.server.serve_forever)]
        for thread in self.threads:thread.start()

    def close(self):
        self.stopped.set()
        if self.threads:self.server.shutdown()
        self.server.server_close()
        for thread in self.threads:thread.join(timeout=3)
