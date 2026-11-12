"""Bounded, best-effort IPv4 IGD mappings for explicitly player-hosted services.

No dependencies, subprocesses, router credentials or Windows-specific adapters.
Existing mappings are inspected and retained; only this handle's unique mapping
may be renewed/deleted. Public relays and joining do not need an inbound mapping.
"""
import http.client
import ipaddress
import os
import secrets
import socket
import threading
import time
from urllib.parse import urljoin, urlsplit
import xml.etree.ElementTree as ET

LEASE = 3600
SERVICES = {f'urn:schemas-upnp-org:service:{name}:{version}'
            for name in ('WANIPConnection', 'WANPPPConnection') for version in (1, 2)}


class MappingError(ValueError):
    def __init__(self, code):
        super().__init__('UPnP mapping error '+str(code))
        self.code = str(code)


def private_ipv4(value):
    try:
        ip = ipaddress.IPv4Address(value)
        return any(ip in ipaddress.IPv4Network(net) for net in
                   ('10.0.0.0/8', '172.16.0.0/12', '192.168.0.0/16'))
    except (ValueError, TypeError):
        return False


def xml(data):
    if len(data) > 65536 or b'<!DOCTYPE' in data.upper() or b'<!ENTITY' in data.upper():
        raise ValueError('Invalid UPnP response')
    return ET.fromstring(data)


def local_url(value, peer):
    url = urlsplit(value)
    if (url.scheme != 'http' or url.hostname != peer or not private_ipv4(peer)
            or url.username or url.password or url.fragment):
        raise ValueError('UPnP control must stay on its responding LAN device')
    return url


def request(url, body=None, action=None):
    # http.client does not follow redirects or inherit proxy configuration.
    connection = http.client.HTTPConnection(url.hostname, url.port or 80, timeout=1)
    try:
        headers = {'Content-Type': 'text/xml; charset="utf-8"'}
        if action:
            headers['SOAPAction'] = '"'+action+'"'
        path = (url.path or '/')+('?' + url.query if url.query else '')
        connection.request('POST' if body else 'GET', path, body, headers)
        response = connection.getresponse()
        data = response.read(65537)
        if response.status not in (200, 500):
            raise ValueError('UPnP HTTP request failed')
        return xml(data)
    finally:
        connection.close()


class Gateway:
    def __init__(self, control, service, local):
        self.control, self.service, self.local = control, service, local

    def call(self, action, values):
        envelope = ET.Element('s:Envelope', {'xmlns:s': 'http://schemas.xmlsoap.org/soap/envelope/'})
        body = ET.SubElement(envelope, 's:Body')
        method = ET.SubElement(body, 'u:'+action, {'xmlns:u': self.service})
        for key, value in values.items():
            ET.SubElement(method, key).text = str(value)
        # Some IGDs reject self-closing empty arguments (notably NewRemoteHost)
        # with402 Invalid Args. Expand them for queries, adds and deletes alike.
        root = request(self.control, ET.tostring(envelope, short_empty_elements=False),
                       self.service+'#'+action)
        error = root.findtext('.//{*}errorCode')
        if error:
            raise MappingError(error)
        response = root.find('.//{*}'+action+'Response')
        if response is None:
            raise ValueError('Missing UPnP action response')
        return {child.tag.rsplit('}', 1)[-1]: child.text or '' for child in response}

    def query(self, port, protocol):
        try:
            return self.call('GetSpecificPortMappingEntry', dict(
                NewRemoteHost='', NewExternalPort=port, NewProtocol=protocol))
        except MappingError as error:
            if error.code != '714':
                raise
            return None


def discover():
    # Select the default IPv4 interface without sending a packet to this address.
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as route:
        route.connect(('192.0.2.1', 9))
        local = route.getsockname()[0]
    if not private_ipv4(local):
        return None
    locations = []
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind((local, 0))
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(local))
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
        sock.settimeout(.25)
        sock.sendto(b'M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\n'
                    b'MAN: "ssdp:discover"\r\nMX: 1\r\nST: ssdp:all\r\n\r\n',
                    ('239.255.255.250', 1900))
        deadline = time.monotonic()+2
        while time.monotonic() < deadline and len(locations) < 4:
            try:
                data, peer = sock.recvfrom(8192)
            except socket.timeout:
                continue
            if not private_ipv4(peer[0]):
                continue
            for line in data.decode('ascii', errors='replace').splitlines():
                if line.lower().startswith('location:'):
                    value = line.split(':', 1)[1].strip()
                    try:
                        url = local_url(value, peer[0])
                    except ValueError:
                        continue
                    if url not in locations:
                        locations.append(url)
    for url in locations:
        try:
            root = request(url)
            base = root.findtext('.//{*}URLBase') or url.geturl()
            local_url(base, url.hostname)
            for service in root.findall('.//{*}service')[:32]:
                kind = service.findtext('{*}serviceType')
                control = service.findtext('{*}controlURL')
                if kind in SERVICES and control:
                    endpoint = local_url(urljoin(base, control), url.hostname)
                    return Gateway(endpoint, kind, local)
        except (OSError, ValueError, ET.ParseError, http.client.HTTPException):
            continue
    return None


class PortMapping:
    def __init__(self, gateway, port, protocol):
        self.gateway, self.port, self.protocol = gateway, port, protocol
        self.description = 'TimeSplitters-'+secrets.token_hex(8)
        self.owned = False
        self.lease = LEASE
        self.stopped = threading.Event()
        self.thread = None
        self.lock = threading.Lock()
        with self.lock:
            self.ensure()

    def same_destination(self, row):
        return (row.get('NewInternalClient') == self.gateway.local
                and row.get('NewInternalPort') == str(self.port)
                and row.get('NewEnabled', '').lower() in ('1', 'true'))

    def mine(self, row):
        return self.same_destination(row) and row.get('NewPortMappingDescription') == self.description

    def ensure(self):
        row = self.gateway.query(self.port, self.protocol)
        if row is not None:
            if not self.same_destination(row):
                raise MappingError('conflicting existing mapping')
            if not self.owned or not self.mine(row):
                # Keep pre-existing/manual/another process's mapping untouched.
                self.owned = False
                return
        values = dict(NewRemoteHost='', NewExternalPort=self.port, NewProtocol=self.protocol,
                      NewInternalPort=self.port, NewInternalClient=self.gateway.local,
                      NewEnabled=1, NewPortMappingDescription=self.description,
                      NewLeaseDuration=self.lease)
        try:
            self.gateway.call('AddPortMapping', values)
        except MappingError as error:
            if error.code != '725':
                raise
            values['NewLeaseDuration'] = self.lease = 0
            self.gateway.call('AddPortMapping', values)
        self.owned = True

    def start(self):
        self.thread = threading.Thread(target=self.renew, daemon=True)
        try:
            self.thread.start()
        except RuntimeError:
            # Failure to maintain a new lease must not leak it or stop hosting.
            self.thread = None
            self.close()
            return None
        return self

    def renew(self):
        # Recheck even existing/permanent mappings so router restarts recover.
        while not self.stopped.wait(300):
            try:
                with self.lock:
                    if not self.stopped.is_set():
                        self.ensure()
            except (OSError, ValueError, ET.ParseError, http.client.HTTPException):
                pass  # Preserve hosting and retry; never overwrite a conflict.

    def close(self):
        self.stopped.set()
        if self.thread:
            self.thread.join(timeout=4)
        try:
            with self.lock:
                if self.owned:
                    row = self.gateway.query(self.port, self.protocol)
                    if row and self.mine(row):
                        values = dict(NewRemoteHost='', NewExternalPort=self.port, NewProtocol=self.protocol)
                        try:
                            self.gateway.call('DeletePortMapping', values)
                        except MappingError as error:
                            if error.code != '402':
                                raise
                            # Some IGDs require a literal wildcard for deletion.
                            # Recheck ownership before retrying the same mapping.
                            row = self.gateway.query(self.port, self.protocol)
                            if row and self.mine(row):
                                values['NewRemoteHost'] = '*'
                                self.gateway.call('DeletePortMapping', values)
                    self.owned = False
        except (OSError, ValueError, ET.ParseError, http.client.HTTPException):
            pass  # Finite leases expire; permanent-only routers need normal exit.


def map_port(port, protocol):
    """Attempt only on explicit host actions. Failure never prevents LAN hosting."""
    if os.environ.get('TSFP_UPNP', '1') == '0':
        return None
    try:
        if type(port) is not int or not 1024 <= port <= 65535 or protocol not in ('TCP', 'UDP'):
            return None
        gateway = discover()
        if gateway:
            return PortMapping(gateway, port, protocol).start()
    except (OSError, ValueError, ET.ParseError, http.client.HTTPException):
        pass
    return None
