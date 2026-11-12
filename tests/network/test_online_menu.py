"""Native controller ABI, real asynchronous cancellation and service routing."""
import struct
import threading
import time
from types import SimpleNamespace
import pytest
import online_menu as menu
import os
from pathlib import Path
import subprocess
from native_compiler import native_compiler

VERSION='a'*64
PIN='b'*64

def request(action,sequence=1,**values):
    return dict(sequence=sequence,action=action,mode=1,source=0,public=True,port=41820,
                name='My Lobby',code='',address='',fingerprint='',**values)

def wait(controller):
    deadline=time.monotonic()+2
    while controller.status==menu.BUSY and time.monotonic()<deadline:time.sleep(.005)
    assert controller.status!=menu.BUSY

def test_binary_abi_is_fixed_and_names_follow_native_font():
    data=menu.REQUEST.pack(menu.MAGIC,menu.VERSION,7,menu.CREATE,1,0,1,41820,
        menu.field('Caf\xe9',64),menu.field('',2048),menu.field('',256),menu.field('',68))
    assert len(data)==2468 and menu.decode_request(data)['name']=='Caf\xe9'
    c=menu.Controller(VERSION)
    assert len(c.snapshot())==7828 and menu.LOBBY.size==244

@pytest.mark.parametrize('field,value',[(0,0),(1,99),(2,0),(3,99),(4,0),(5,2),(6,2),(7,80)])
def test_invalid_abi_never_dispatches(field,value):
    header=[menu.MAGIC,menu.VERSION,1,menu.BROWSE,1,0,1,41820];header[field]=value
    data=menu.REQUEST.pack(*header,*[menu.field('',n) for n in (64,2048,256,68)])
    with pytest.raises(ValueError):menu.decode_request(data)

def test_unterminated_field_and_trailing_data_rejected():
    data=menu.REQUEST.pack(menu.MAGIC,menu.VERSION,1,menu.BROWSE,1,0,1,41820,b'x'*64,*[bytes(n) for n in (2048,256,68)])
    with pytest.raises(ValueError):menu.decode_request(data)
    with pytest.raises(ValueError):menu.decode_request(data+b'x')

def test_private_invitation_is_a_server_not_a_lobby():
    value=menu.server_invitation('https://private.example:41820',PIN)
    assert menu.parse_server_invitation(value)==('https://private.example:41820',PIN)
    for bad in ('',value+'!',menu.invitation('https://private.example',PIN,'A'*16)):
        with pytest.raises(ValueError):menu.parse_server_invitation(bad)
    with pytest.raises(ValueError):menu.server_invitation('http://127.0.0.1','')

def test_browser_lists_arcade_and_story_without_changing_community(monkeypatch):
    calls=[]
    def client(address,pin):
        def rpc(method,path):
            calls.append((address,path))
            return {'lobbies':[dict(code='2'*16,compatibility=VERSION,mode='story',players=1,capacity=4)]}
        return SimpleNamespace(request=rpc,url=SimpleNamespace(scheme='https'),fingerprint=pin)
    monkeypatch.setattr(menu,'Client',client)
    monkeypatch.setattr(menu,'list_arcade',lambda c,v:{'lobbies':[dict(code='1'*16,compatibility=v,mode='arcade',players=2,capacity=8)]})
    c=menu.Controller(VERSION);c.community=('https://community.example',PIN)
    c.private=('https://private.example',PIN)
    r=request(menu.BROWSE);r['source']=1;c.begin(r);wait(c)
    assert c.status==menu.LIST and len(c.lobbies)==2
    assert calls[0][0]=='https://private.example' and c.community[0]=='https://community.example'
    raw=c.snapshot();assert struct.unpack_from('<I',raw,20)[0]==2
    assert menu.LOBBY.unpack_from(raw,menu.STATE.size+menu.LOBBY.size)[3]==2
    c.close()

def test_cancelled_connection_is_closed_and_never_published(monkeypatch):
    entered=threading.Event();release=threading.Event();closed=threading.Event()
    active=SimpleNamespace(close=closed.set)
    def connect(*args,**kwargs):entered.set();assert release.wait(2);return active
    monkeypatch.setattr(menu.ArcadeSession,'open',connect)
    c=menu.Controller(VERSION);c.community=('https://community.example',PIN)
    c.begin(request(menu.CREATE));assert entered.wait(1)
    c.begin(request(menu.CANCEL,2));wait(c);release.set()
    assert closed.wait(1) and c.session is None and c.status==menu.IDLE
    assert menu.STATE.unpack_from(c.snapshot())[3]==menu.IDLE
    c.close()

def test_failed_request_returns_to_native_menu_with_a_reason(monkeypatch):
    def connect(*args,**kwargs):raise ValueError('The match is full')
    monkeypatch.setattr(menu,'join_arcade',connect)
    c=menu.Controller(VERSION);c.community=('https://community.example',PIN)
    r=request(menu.JOIN);r['code']='A'*16;c.begin(r);wait(c)
    assert c.status==menu.ERROR and c.message=='The match is full' and c.session is None
    c.close()

def test_lobby_leave_keeps_owned_private_directory_running():
    events=[]
    process=SimpleNamespace(terminate=lambda:events.append('stop server'),wait=lambda **kw:None,poll=lambda:None)
    c=menu.Controller(VERSION);c.private_process=process;c.private=('https://private.example',PIN)
    c.session=SimpleNamespace(close=lambda:events.append('leave lobby'))
    c.begin(request(menu.LEAVE));wait(c)
    assert events==['leave lobby'] and c.private_process is process
    c.close();assert events==['leave lobby','stop server']

def test_stale_native_requests_do_not_repeat_actions():
    c=menu.Controller(VERSION);c.begin(request(menu.CANCEL,2));wait(c)
    generation=c.generation;c.begin(request(menu.CREATE,1))
    assert c.generation==generation and c.sequence==2 and c.status==menu.IDLE
    c.close()

@pytest.mark.parametrize('fixture',['online_menu_ipc.c','multiplayer_exit.c'])
def test_production_native_ipc_and_retained_return(tmp_path,fixture):
    compiler,environment=native_compiler(tmp_path)
    root=Path(__file__).resolve().parents[2]
    exe=tmp_path/('online-ipc.exe' if os.name=='nt' else 'online-ipc')
    source=root/'tests/network'/fixture
    if Path(compiler).name.lower() in ('cl','cl.exe'):
        argv=[compiler,'/nologo','/std:c11','/W4','/I'+str(root/'src'),str(source),'/Fe:'+str(exe)]
    else:argv=[compiler,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(root/'src'),str(source),'-o',str(exe)]
    build=subprocess.run(argv,cwd=tmp_path,env=environment,capture_output=True,text=True,timeout=60)
    assert build.returncode==0,build.stdout+build.stderr
    result=subprocess.run([str(exe),str(tmp_path/'1.ready')],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr


def test_match_details_are_validated_and_only_published_by_current_host():
    from online_arcade_client import NativeArcade
    bridge=SimpleNamespace(peer_id=7)
    native=NativeArcade(bridge);native.status=dict(epoch=1,host_peer=7,phase='lobby')
    active=SimpleNamespace(host=True,native=native)
    c=menu.Controller(VERSION);c.session=active;c.mode=1
    data=menu.MATCH.pack(menu.MAGIC,menu.VERSION,1,48,2,1,menu.field('Temple',64),menu.field('Deathmatch',32))
    c.update_metadata(data)
    assert native.body(VERSION)['match_info']['map_scene']==48
    native.status['host_peer']=9
    assert 'match_info' not in native.body(VERSION)
    active.host=False;native.match_info=None;c.update_metadata(data)
    assert native.match_info is None
    c.mode=2;active.host=True;c.update_metadata(data)
    assert native.match_info is None
    for changed in (data+b'x',data[:12]+struct.pack('<I',101)+data[16:],
                    data[:20]+struct.pack('<I',2)+data[24:]):
        with pytest.raises(ValueError):c.update_metadata(changed)
    c.session=None;c.close()


def test_switching_private_servers_does_not_redirect_to_owned_loopback(monkeypatch):
    calls=[]
    monkeypatch.setattr(menu,'Client',lambda *pair:calls.append(pair) or pair)
    c=menu.Controller(VERSION);c.private_process=SimpleNamespace(poll=lambda:None)
    c.private_hosted=('https://mine.example',PIN);c.private_local=('https://127.0.0.1:41830',PIN)
    c.private=c.private_hosted;c.directory(1)
    c.private=('https://friend.example',PIN);c.directory(1)
    assert calls==[c.private_local,c.private]
    c.begin(request(menu.PRIVATE_HOST));wait(c)
    assert c.private==c.private_hosted and c.status==menu.PRIVATE_READY
    c.private_process=None;c.close()


@pytest.mark.parametrize('local_matches', [True, False])
def test_private_invitation_uses_local_endpoint_only_after_pin_verification(monkeypatch, local_matches):
    from urllib.parse import urlsplit
    calls=[]
    class Probe:
        def __init__(self, address, pin):
            self.url=urlsplit(address);self.fingerprint=pin
        def request(self, method, path, **options):
            calls.append((self.url.geturl(), self.fingerprint, options.get('timeout')))
            if self.url.hostname=='127.0.0.1' and not local_matches:
                raise ValueError('Directory certificate does not match the supplied fingerprint')
            return {'private_directory': True}
    monkeypatch.setattr(menu, 'Client', Probe)
    c=menu.Controller(VERSION)
    pair=('https://private.example:41830', PIN)
    r=request(menu.PRIVATE_CONNECT);r['source']=1;r['code']=menu.server_invitation(*pair)
    c.begin(r);wait(c)
    assert c.status==menu.PRIVATE_READY and c.private==pair
    expected='https://127.0.0.1:41830' if local_matches else pair[0]
    assert c.directory(1).url.geturl()==expected
    assert calls[0]==('https://127.0.0.1:41830', PIN, 1)
    assert len(calls)==(1 if local_matches else 2)
    c.private=('https://another.example:41830', PIN)
    assert c.directory(1).url.geturl()==c.private[0]
    c.close()


@pytest.mark.parametrize('supported',[False,True])
def test_locked_creation_requires_visible_admission_contract(monkeypatch,supported):
    created=[]
    directory=SimpleNamespace(request=lambda *a,**k:{'invite_only_listing':supported})
    c=menu.Controller(VERSION)
    monkeypatch.setattr(c,'directory',lambda source:directory)
    active=SimpleNamespace(close=lambda:None)
    monkeypatch.setattr(menu.ArcadeSession,'open',lambda *a,**k:created.append(k) or active)
    r=request(menu.CREATE);r['public']=False;c.begin(r);wait(c)
    assert bool(created)==supported
    assert c.status==(menu.CONNECTED if supported else menu.ERROR)
    if not supported:assert 'update' in c.message
    c.close()


def test_selected_locked_lobby_cannot_redirect_to_another_directory(monkeypatch):
    c=menu.Controller(VERSION)
    r=request(menu.JOIN);r['address']='1'*16;r['code']=menu.invitation('https://other.example',PIN,'2'*16)
    c.begin(r);wait(c)
    assert c.status==menu.ERROR and c.message=='Enter the code for the selected lobby'
    c.close()


@pytest.mark.parametrize('kind',['v2','v3'])
def test_selected_lobby_id_and_secret_are_separate_admission_fields(kind):
    from online_client import Session,DirectoryError
    from online_arcade_client import join_arcade
    calls=[]
    def rpc(method,path,body):
        calls.append((method,path,body))
        raise DirectoryError(403,'Incorrect lobby code')
    client=SimpleNamespace(request=rpc)
    with pytest.raises(DirectoryError) as error:
        if kind=='v2':Session.join(client,VERSION,'B'*16,lobby_id='A'*16)
        else:join_arcade(client,VERSION,'B'*16,lobby_id='A'*16)
    assert error.value.status==403 and len(calls)==1
    assert calls[0][1]=='/'+kind+'/lobbies/'+'A'*16+'/join'
    assert calls[0][2]['lobby_code']=='B'*16
