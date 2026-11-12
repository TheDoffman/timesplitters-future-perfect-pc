"""IGD behavior and hosting lifetimes; no test changes a physical router."""
import socket
from types import SimpleNamespace
from urllib.parse import urlsplit
import xml.etree.ElementTree as ET
import pytest
import online_upnp as upnp


class Gateway:
    local = '192.168.1.10'
    def __init__(self, row=None, permanent=False):
        self.row = row
        self.calls = []
        self.permanent = permanent
    def query(self, port, protocol):
        return self.row
    def call(self, action, values):
        self.calls.append((action, dict(values)))
        if action == 'AddPortMapping':
            if self.permanent and values['NewLeaseDuration']:
                raise upnp.MappingError(725)
            self.row = {key: str(value) for key, value in values.items()}
        elif action == 'DeletePortMapping':
            self.row = None
        return {}


def manual(**values):
    return dict(NewInternalClient='192.168.1.10', NewInternalPort='30820',
                NewEnabled='1', NewPortMappingDescription='Manual forward', **values)


def test_owned_mapping_has_bounded_lease_and_cleans_up_only_itself():
    router = Gateway()
    lease = upnp.PortMapping(router, 30820, 'TCP')
    assert lease.owned and router.row['NewLeaseDuration'] == '3600'
    assert router.row['NewInternalClient'] == router.local
    assert router.row['NewExternalPort'] == '30820' and router.row['NewProtocol'] == 'TCP'
    assert router.row['NewPortMappingDescription'].startswith('TimeSplitters-')
    lease.close();lease.close()
    assert [call[0] for call in router.calls] == ['AddPortMapping','DeletePortMapping']


def test_existing_manual_mapping_is_retained_without_renewing_or_deleting():
    router = Gateway(manual())
    lease = upnp.PortMapping(router,30820,'TCP')
    lease.ensure();lease.close()
    assert not router.calls and router.row['NewPortMappingDescription'] == 'Manual forward'


@pytest.mark.parametrize('key,value', [('NewInternalClient','192.168.1.9'),
    ('NewInternalPort','12345'),('NewEnabled','0')])
def test_conflicting_mapping_is_never_overwritten(key,value):
    row=manual();row[key]=value;router=Gateway(row)
    with pytest.raises(upnp.MappingError):upnp.PortMapping(router,30820,'TCP')
    assert not router.calls and router.row == row


def test_permanent_only_router_is_supported_and_owned_mapping_removed_on_exit():
    router=Gateway(permanent=True);lease=upnp.PortMapping(router,30821,'UDP')
    assert [c[1]['NewLeaseDuration'] for c in router.calls] == [3600,0]
    assert lease.lease == 0
    lease.close();assert router.calls[-1][0] == 'DeletePortMapping'


def test_router_restart_recreates_missing_mapping_and_preserves_replacement():
    router=Gateway();lease=upnp.PortMapping(router,30820,'TCP')
    router.row=None;lease.ensure()
    assert len(router.calls)==2
    router.row=manual();lease.ensure();lease.close()
    assert len(router.calls)==2 and router.row['NewPortMappingDescription']=='Manual forward'


def test_replacement_on_another_device_is_not_deleted():
    router=Gateway();lease=upnp.PortMapping(router,30820,'TCP')
    router.row['NewInternalClient']='192.168.1.99'
    lease.close();assert len(router.calls)==1


def test_disable_and_unavailable_igd_are_nonfatal(monkeypatch):
    def forbidden():raise AssertionError('disabled tests must not discover physical routers')
    monkeypatch.setattr(upnp,'discover',forbidden)
    assert upnp.map_port(30820,'TCP') is None
    monkeypatch.setenv('TSFP_UPNP','1')
    monkeypatch.setattr(upnp,'discover',lambda:None)
    assert upnp.map_port(30820,'TCP') is None
    def denied():raise OSError('router disabled')
    monkeypatch.setattr(upnp,'discover',denied)
    assert upnp.map_port(30820,'TCP') is None


def test_mapping_factory_starts_and_stops_maintenance_without_leaking_thread(monkeypatch):
    monkeypatch.setenv('TSFP_UPNP','1')
    router=Gateway();monkeypatch.setattr(upnp,'discover',lambda:router)
    lease=upnp.map_port(30820,'TCP')
    try:
        assert lease is not None and lease.thread.is_alive()
    finally:
        if lease:lease.close()
    assert not lease.thread.is_alive() and router.row is None


def test_maintenance_start_failure_cleans_mapping_and_preserves_hosting(monkeypatch):
    monkeypatch.setenv('TSFP_UPNP','1')
    router=Gateway();monkeypatch.setattr(upnp,'discover',lambda:router)
    def fail():raise RuntimeError('cannot start worker')
    monkeypatch.setattr(upnp,'threading',SimpleNamespace(Event=upnp.threading.Event,
        Lock=upnp.threading.Lock,Thread=lambda **kwargs:SimpleNamespace(start=fail)))
    assert upnp.map_port(30820,'TCP') is None and router.row is None
    assert [c[0] for c in router.calls]==['AddPortMapping','DeletePortMapping']


@pytest.mark.parametrize('value', ['http://8.8.8.8/root.xml',
    'http://192.168.1.2/root.xml','http://name.example/root.xml',
    'http://user:password@192.168.1.1/root.xml','https://192.168.1.1/root.xml'])
def test_discovery_cannot_redirect_to_public_other_or_authenticated_hosts(value):
    with pytest.raises(ValueError):upnp.local_url(value,'192.168.1.1')


@pytest.mark.parametrize('data',[b'x'*65537,b'<!DOCTYPE x><x/>',b'<!ENTITY x "abc"><x/>'], ids=['oversize','doctype','entity'])
def test_xml_is_bounded_and_disallows_entity_declarations(data):
    with pytest.raises(ValueError):upnp.xml(data)


def test_soap_encodes_actions_parses_fault_and_no_such_entry(monkeypatch):
    calls=[]
    def rpc(url,body,action):
        calls.append((ET.fromstring(body),action))
        if action.endswith('GetSpecificPortMappingEntry'):
            return ET.fromstring('<Envelope><errorCode>714</errorCode></Envelope>')
        return ET.fromstring('<Envelope><AddPortMappingResponse/></Envelope>')
    monkeypatch.setattr(upnp,'request',rpc)
    router=upnp.Gateway(urlsplit('http://192.168.1.1/control'),
        'urn:schemas-upnp-org:service:WANIPConnection:2','192.168.1.10')
    assert router.query(30820,'TCP') is None
    router.call('AddPortMapping',{'NewPortMappingDescription':'A & B'})
    assert calls[-1][0].findtext('.//NewPortMappingDescription')=='A & B'
    assert calls[-1][1].endswith('WANIPConnection:2#AddPortMapping')


def test_discovery_binds_default_interface_and_rejects_unrelated_locations(monkeypatch):
    import itertools
    packets=iter([(b'LOCATION: http://8.8.8.8/root\r\n',('8.8.8.8',1900)),
                  (b'LOCATION: http://8.8.8.8/root\r\n',('192.168.1.1',1900)),
                  (b'LOCATION: http://192.168.1.1/root\r\n',('192.168.1.1',1900))])
    sent=[]
    class Socket:
        def __enter__(self):return self
        def __exit__(self,*args):pass
        def connect(self,peer):pass
        def getsockname(self):return ('192.168.1.10',12345)
        def bind(self,peer):assert peer==('192.168.1.10',0)
        def setsockopt(self,*args):pass
        def settimeout(self,*args):pass
        def sendto(self,data,peer):sent.append((data,peer))
        def recvfrom(self,*args):return next(packets)
    monkeypatch.setattr(upnp.socket,'socket',lambda *args:Socket())
    clock=itertools.count(0,.5);monkeypatch.setattr(upnp,'time',SimpleNamespace(monotonic=lambda:next(clock)))
    def description(url):
        assert url.hostname=='192.168.1.1'
        return ET.fromstring('<root xmlns="urn:schemas-upnp-org:device-1-0"><device><serviceList>'
            '<service><serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType>'
            '<controlURL>/control</controlURL></service></serviceList></device></root>')
    monkeypatch.setattr(upnp,'request',description)
    router=upnp.discover()
    assert router.local=='192.168.1.10' and router.control.geturl()=='http://192.168.1.1/control'
    assert sent[0][1]==('239.255.255.250',1900) and b'ST: ssdp:all' in sent[0][0]


def test_http_control_does_not_follow_redirects_and_closes_connection(monkeypatch):
    seen=[]
    class Connection:
        def __init__(self,host,port,timeout):assert timeout==1
        def request(self,method,path,body,headers):seen.append((method,path,headers))
        def getresponse(self):return SimpleNamespace(status=302,read=lambda size:b'<root/>')
        def close(self):seen.append('closed')
    monkeypatch.setattr(upnp.http.client,'HTTPConnection',Connection)
    with pytest.raises(ValueError):upnp.request(urlsplit('http://192.168.1.1/control'),b'<request/>','action')
    assert seen[0][0]=='POST' and seen[0][2]['SOAPAction']=='"action"' and seen[-1]=='closed'


def test_renew_loop_recovers_and_stops_before_network_on_close():
    router=Gateway();lease=upnp.PortMapping(router,30820,'TCP')
    class Stop:
        calls=0
        def wait(self,seconds):
            self.calls+=1
            if self.calls==1:router.row=None;return False
            return True
        def is_set(self):return False
    lease.stopped=Stop();lease.renew()
    assert len(router.calls)==2


def test_only_directory_owner_maps_ports_and_lobby_leave_retains_both(tmp_path,monkeypatch):
    import online_private,online_client
    from test_online_private import udp_port
    handles=[]
    def mapped(port,protocol):
        handle=SimpleNamespace(port=port,protocol=protocol,closed=False)
        def close():handle.closed=True
        handle.close=close;handles.append(handle);return handle
    monkeypatch.setattr(online_private,'map_port',mapped)
    monkeypatch.setattr(online_client,'map_port',mapped)
    directory=online_private.LocalDirectory(tmp_path,'192.0.2.1',0,state_folder=tmp_path/'cert')
    host=guest=None
    try:
        assert handles[0].protocol=='TCP' and handles[0].port==directory.server.server_port
        client=online_client.Client(directory.address,directory.fingerprint)
        host=online_client.Session.create(client,'a'*64,'Host',udp_port(),True,transport='relay')
        assert handles[1].protocol=='UDP' and handles[1].port==host.lobby['port']
        guest=online_client.Session.join(client,'a'*64,host.lobby['code'])
        assert len(handles)==2  # joining never opens an inbound port
        guest.close();guest=None
        host.close();host=None
        assert not any(h.closed for h in handles) and directory.poll() is None
    finally:
        if guest:guest.close()
        if host:host.close()
        directory.close()
    assert all(h.closed for h in handles)


def test_failed_lobby_admission_rolls_back_new_udp_mapping(monkeypatch):
    import online_client
    from test_online_private import udp_port
    handles=[]
    monkeypatch.setattr(online_client,'map_port',lambda port,protocol:
        SimpleNamespace(close=lambda:handles.append('closed')))
    def denied(*args,**kwargs):raise ValueError('directory unavailable')
    with pytest.raises(ValueError,match='directory unavailable'):
        online_client.Session.create(SimpleNamespace(request=denied),'a'*64,'Host',udp_port(),True)
    assert handles==['closed']


def test_relay_hosting_does_not_create_inbound_router_mappings(monkeypatch):
    import online_client
    def forbidden(*args):raise AssertionError('relay hosting is outbound only')
    monkeypatch.setattr(online_client,'map_port',forbidden)
    class Bridge:
        def __init__(self,*args,**kwargs):self.ready=SimpleNamespace(wait=lambda seconds:True)
        def close(self):pass
    class Hosted(online_client.Session):
        def __init__(self,client,compatibility,lobby,host,bridge,mode):self.bridge=bridge
    monkeypatch.setattr(online_client,'Bridge',Bridge)
    lobby=dict(mode='arcade',capacity=16,transport='relay',address='192.0.2.1',
               port=41821,relay_key='b'*64,room='a'*32)
    session=Hosted.create(SimpleNamespace(request=lambda *args,**kwargs:lobby),
        'a'*64,'Relay host',30821,True,transport='relay')
    assert session.port_mapping is None
