"""Headless player-side controller for the game's native multiplayer menus."""
import base64
import json
import os
from pathlib import Path
import struct
import subprocess
import threading
import time

from online_client import Client, DirectoryError, Session
from online_arcade_client import ArcadeSession, list_arcade, join_arcade
from online_host import invitation, parse_invitation, lan_ipv4
from online_paths import ROOT, service_defaults

MAGIC=0x314d4f54
VERSION=3
LIMIT=20
REQUEST=struct.Struct('<8I64s2048s256s68s')
STATE=struct.Struct('<8I256s32s2048s256s256s68s')
LOBBY=struct.Struct('<7I32s64s64s32s24s')
MATCH=struct.Struct('<6I64s32s')
ROSTER_HEADER=struct.Struct('<7I')
PERSON=struct.Struct('<Iiii32s48s')
IDLE,BUSY,LIST,CONNECTED,PRIVATE_READY,ERROR=range(6)
BROWSE,CREATE,JOIN,LEAVE,PRIVATE_HOST,PRIVATE_CONNECT,CANCEL=range(1,8)

def field(value,size):
    value=str(value).replace('\0','').replace('\r',' ').replace('\n',' ')
    return value.encode('latin-1',errors='replace')[:size-1].ljust(size,b'\0')

def text(value):
    if b'\0' not in value:raise ValueError('Unterminated native menu field')
    return value.split(b'\0',1)[0].decode('latin-1')

def decode_request(data):
    if len(data)!=REQUEST.size:raise ValueError('Invalid native menu request size')
    magic,version,seq,action,mode,source,public,port,name,code,address,pin=REQUEST.unpack(data)
    if (magic!=MAGIC or version!=VERSION or not seq or action not in range(1,8) or
        mode not in (1,2) or source not in (0,1) or public not in (0,1) or not 1024<=port<=65535):
        raise ValueError('Invalid native menu request')
    return dict(sequence=seq,action=action,mode=mode,source=source,public=bool(public),
                port=port,name=text(name),code=text(code).strip(),address=text(address),fingerprint=text(pin))

def decode_match(data):
    if len(data)!=MATCH.size:raise ValueError('Invalid match details size')
    magic,version,mode,scene,game_type,active,name,rules=MATCH.unpack(data)
    if magic!=MAGIC or version!=VERSION or mode not in (1,2) or scene>100 or game_type>=32 or active>1:
        raise ValueError('Invalid match details')
    name,rules=text(name),text(rules)
    if not name or not rules or any(ord(c)<32 or ord(c)==127 for c in name+rules):
        raise ValueError('Invalid match labels')
    return mode,dict(map_scene=scene,game_type_id=game_type,in_progress=bool(active),map_name=name,game_type=rules)

def decode_roster(data):
    if len(data)!=ROSTER_HEADER.size+16*PERSON.size:raise ValueError('Invalid player report size')
    magic,version,scene,tick,generation,count,serial=ROSTER_HEADER.unpack_from(data)
    if magic!=0x31524f54 or version!=1 or scene>100 or generation>255 or not 1<=count<=16 or not serial:
        raise ValueError('Invalid player report')
    people=[];seen=set()
    for index in range(count):
        slot,character,skin,portrait,name,label=PERSON.unpack_from(data,ROSTER_HEADER.size+index*PERSON.size)
        if slot>=16 or slot in seen or not -1<=character<4096 or not -128<=skin<=127 or not -1<=portrait<1024:
            raise ValueError('Invalid player identity')
        name,label=text(name),text(label)
        if any(ord(c)<32 or ord(c)==127 for c in name+label):raise ValueError('Invalid identity label')
        seen.add(slot);people.append(dict(slot=slot,name=name,character=character,skin=skin,portrait=portrait,character_name=label))
    return dict(scene=scene,tick=tick,generation=generation,serial=serial,people=people)


def invitation_lan_addresses(values):
    if not isinstance(values,(list,tuple)) or len(values)>4 or any(not isinstance(v,str) or not lan_ipv4(v) for v in values):
        raise ValueError('Invalid private server local addresses')
    return tuple(dict.fromkeys(values))


def server_invitation(address,pin,local_addresses=()):
    directory=Client(address,pin)
    if directory.url.scheme!='https' or not directory.fingerprint:
        raise ValueError('Private servers require HTTPS and a certificate fingerprint')
    data=dict(directory=address,fingerprint=directory.fingerprint)
    local=invitation_lan_addresses(local_addresses)
    if local:data['local_addresses']=local
    raw=json.dumps(data,separators=(',',':')).encode()
    return 'tsfp-server:'+base64.urlsafe_b64encode(raw).decode().rstrip('=')

def parse_server_details(value):
    if not isinstance(value,str) or not value.startswith('tsfp-server:') or len(value)>2048:
        raise ValueError('Enter the private server invitation supplied by its owner')
    try:
        encoded=value[len('tsfp-server:'):]
        data=json.loads(base64.b64decode(encoded+'='*(-len(encoded)%4),altchars=b'-_',validate=True))
        if not isinstance(data,dict):raise ValueError('Invalid private server invitation')
        local=invitation_lan_addresses(data.get('local_addresses',()))
        server_invitation(data['directory'],data['fingerprint'],local)
        return (data['directory'],data['fingerprint']),local
    except (ValueError,KeyError,TypeError) as error:
        raise ValueError('Invalid private server invitation') from error

def parse_server_invitation(value):
    return parse_server_details(value)[0]


def stop_private(process):
    """Ask the actual server to stop, including Windows venv launcher children."""
    stop=getattr(process,'tsfp_stop_path',None)
    if stop:
        stop.write_bytes(b'')
        try:process.wait(timeout=5);return
        except subprocess.TimeoutExpired:pass
    process.terminate()
    try:process.wait(timeout=5)
    except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)

class Controller:
    def __init__(self,compatibility,private_launcher=None):
        self.compatibility=compatibility
        defaults=service_defaults()
        self.community=(defaults['directory'],defaults['fingerprint'])
        self.private=None
        self.private_connected=None
        self.private_direct=False
        self.private_relay=False
        self.private_local=None
        self.private_hosted=None
        self.source=0
        self.active_public=None
        self.private_launcher=private_launcher
        self.private_process=None
        self.session=None
        self.sequence=0
        self.mode=1
        self.status=IDLE
        self.message='Choose a network'
        self.lobbies=[]
        self.lock=threading.RLock()
        self.generation=0
        self.closed=False

    def close_session(self):
        session,self.session=self.session,None
        self.active_public=None
        if session:session.close()

    def close(self):
        with self.lock:
            self.closed=True;self.generation+=1
            self.close_session()
            process,self.private_process=self.private_process,None
        if process:
            stop_private(process)

    def directory(self,source):
        pair=self.private if source else self.community
        if not pair:raise ValueError('Connect to a private server first')
        endpoint=pair
        if source:
            if self.private_process and self.private==self.private_hosted and self.private_local:
                endpoint=self.private_local
            elif self.private_connected and self.private_connected[0]==pair:
                endpoint=self.private_connected[1]
        return Client(*endpoint)

    def begin(self,request):
        with self.lock:
            if request['sequence']<=self.sequence:return
            self.sequence=request['sequence'];self.mode=request['mode'];self.source=request['source']
            self.generation+=1;generation=self.generation
            self.status=BUSY;self.message='Connecting...' if request['action']!=BROWSE else 'Refreshing matches...'
        worker=threading.Thread(target=self.perform,args=(request,generation),daemon=True)
        worker.start()

    def perform(self,request,generation):
        active=None
        try:
            with self.lock:
                if self.closed or generation!=self.generation:return
            action=request['action'];mode='arcade' if request['mode']==1 else 'story'
            if action in (CANCEL,LEAVE):
                with self.lock:self.close_session()
                result=(IDLE,'Choose a match',[])
            elif action==PRIVATE_CONNECT:
                pair,local_addresses=parse_server_details(request['code'])
                probe=Client(*pair);endpoint=pair
                path='/v2/lobbies?compatibility='+self.compatibility+'&mode='+mode
                # Every alternative must authenticate the invitation's same pin.
                # LAN hints avoid router hairpin NAT for another home device;
                # the public endpoint remains the fallback for remote friends.
                port=str(probe.url.port or 443)
                candidates=['https://127.0.0.1:'+port]
                candidates += ['https://'+address+':'+port for address in local_addresses]
                for address in dict.fromkeys(candidates):
                    try:
                        reply=Client(address,pair[1]).request('GET',path,timeout=1)
                        endpoint=(address,pair[1]);break
                    except DirectoryError:
                        raise
                    except (OSError,ValueError):
                        continue
                else:
                    reply=probe.request('GET',path)
                with self.lock:
                    if generation==self.generation:
                        self.private=pair;self.private_direct=reply.get('private_directory') is True
                        self.private_relay=reply.get('private_relay') is True
                        self.private_connected=(pair,endpoint)
                result=(PRIVATE_READY,'Connected to private server',[])
            elif action==PRIVATE_HOST:
                with self.lock:
                    existing=self.private_process if self.private_process and self.private_process.poll() is None else None
                    if existing:
                        self.private=self.private_hosted;self.private_direct=bool(getattr(existing,'direct',False))
                        self.private_relay=bool(getattr(existing,'relay',False))
                if not existing:
                    if not self.private_launcher:raise ValueError('Private directory could not start')
                    process,pair,local=self.private_launcher(request)
                    with self.lock:
                        if generation!=self.generation or self.closed:
                            stop_private(process);return
                        self.private_process=process;self.private=self.private_hosted=pair;self.private_local=local
                        self.private_direct=bool(getattr(process,'direct',False))
                        self.private_relay=bool(getattr(process,'relay',False))
                mapped=bool(getattr(self.private_process,'port_mapping',None) and getattr(self.private_process,'relay_mapping',None))
                message='Private server ready. Router mapping active.' if mapped else 'Private server ready. Automatic router mapping unavailable; forward its TCP directory and UDP relay ports for Internet guests.'
                result=(PRIVATE_READY,message,[])
            elif action==BROWSE:
                with self.lock:self.close_session()
                directory=self.directory(request['source'])
                reply=list_arcade(directory,self.compatibility)
                story=directory.request('GET','/v2/lobbies?compatibility='+self.compatibility+'&mode=story')
                rows=[r for r in reply['lobbies']+story['lobbies'] if r.get('compatibility')==self.compatibility]
                result=(LIST,f'{len(rows)} matches available',rows[:LIMIT])
            elif action in (CREATE,JOIN):
                with self.lock:
                    if self.session:raise ValueError('Leave the current lobby first')
                directory=self.directory(request['source'])
                if action==CREATE and not request['public']:
                    capabilities=directory.request('GET','/v2/lobbies?compatibility='+self.compatibility+'&mode='+mode)
                    if capabilities.get('invite_only_listing') is not True:
                        raise ValueError('This server needs an update for visible Invite Only lobbies')
                if action==JOIN:
                    code=request['code']
                    if code.startswith('tsfp-online:'):
                        if request['address']:raise ValueError('Enter the code for the selected lobby')
                        address,pin,code=parse_invitation(code);directory=Client(address,pin)
                    selected={'lobby_id':request['address']} if request['address'] else {}
                    active=(join_arcade(directory,self.compatibility,code,**selected) if mode=='arcade' else
                            Session.join(directory,self.compatibility,code,mode=mode,**selected))
                elif request['source'] and self.private_direct:
                    active=Session.create(directory,self.compatibility,request['name'] or 'My Lobby',request['port'],request['public'],mode=mode,transport='relay' if self.private_relay else 'direct')
                elif mode=='arcade':
                    active=ArcadeSession.open(directory,self.compatibility,name=request['name'] or 'My Lobby',public=request['public'])
                else:
                    active=Session.create(directory,self.compatibility,request['name'] or 'My Lobby',request['port'],request['public'],mode=mode,transport='relay')
                result=(CONNECTED,'Lobby connected',[])
            else:raise ValueError('Unknown native menu action')
            with self.lock:
                if self.closed or generation!=self.generation:
                    if active:active.close()
                    return
                if active:
                    self.session=active
                    self.active_public=self.private if request['source'] else None
                self.status,self.message,self.lobbies=result
        except Exception as error:
            if active:active.close()
            with self.lock:
                if not self.closed and generation==self.generation:
                    self.status=ERROR;self.message=str(error)

    def update_metadata(self,data):
        mode,info=decode_match(data)
        with self.lock:
            active=self.session
            if not active or not active.host or mode!=self.mode:return
            native=getattr(active,'native',None)
            if native:
                with native.lock:native.match_info=info
            else:active.match_info=info

    def update_players(self,data):
        info=decode_roster(data)
        with self.lock:
            active=self.session
            native=getattr(active,'native',None)
            if not native or self.mode!=1:return
            with native.lock:
                if not native.status or native.status['host_peer']!=native.bridge.peer_id:return
                native.roster_info=info
                native.roster_at=native.bridge.clock()

    def snapshot(self):
        with self.lock:
            active=self.session
            if self.private_process and self.private_process.poll() is not None:
                selected=self.private==self.private_hosted
                self.private_process=None;self.private_local=None;self.private_hosted=None
                if selected:self.private=None
                if selected and self.source:
                    self.status=ERROR;self.message='Your private server stopped'
            if active and active.closed.is_set():
                self.message=active.error or 'The host closed the lobby'
                self.status=ERROR;self.close_session();active=None
            pair=self.private or ('','')
            code=active.lobby.get('lobby_code',active.lobby['code']) if active else ''
            local=getattr(self.private_process,'lan_addresses',()) if self.private==self.private_hosted else ()
            invite=server_invitation(*pair,local_addresses=local) if pair[0] else ''
            if active:
                directory,pin=self.active_public or (active.client.url.geturl(),active.client.fingerprint)
                invite=invitation(directory,pin,code)
            raw=STATE.pack(MAGIC,VERSION,self.sequence,self.status,self.mode,len(self.lobbies),
                int(bool(active and active.host)),int(self.private_process is not None and self.private==self.private_hosted),
                field(self.message,256),field(code,32),field(invite,2048),field(active.bridge.config if active else '',256),
                field(pair[0],256),field(pair[1],68))
            for row in self.lobbies:
                raw+=LOBBY.pack(int(row.get('players',0)),int(row.get('capacity',16)),int(row.get('joinable',True)),
                    2 if row.get('mode')=='story' else 1,int(row.get('map_scene',0xffffffff)),int(row.get('game_type_id',0xffffffff)),int(bool(row.get('invite_only',False))),field(row['code'],32),field(row.get('name','Lobby'),64),
                    field(row.get('map_name',''),64),field(row.get('game_type',row.get('mode','arcade')),32),field(row.get('phase','lobby'),24))
            return raw+bytes((LIMIT-len(self.lobbies))*LOBBY.size)

def private_launcher(result,parent):
    def launch(request):
        command=os.environ.get('TSFP_PRIVATE_SERVER_COMMAND')
        if not command:
            from online_private import start_directory
            directory=start_directory(ROOT,request)
            return directory,(directory.public_url,directory.fingerprint),(directory.address,directory.fingerprint)
        if command:
            argv=json.loads(command)
            if not isinstance(argv,list) or not argv or not all(isinstance(v,str) and v for v in argv):
                raise ValueError('Invalid private hosting component command')
        target=Path(str(result)+'.server-'+str(request['sequence']))
        target.unlink(missing_ok=True)
        stop=Path(str(target)+'.stop');stop.unlink(missing_ok=True)
        argv += ['--parent',str(os.getpid()),'--result',str(target),'--port',str(request['port']),
                 '--client-root',str(Path(__file__).resolve().parent)]
        if request['address']:argv+=['--address',request['address']]
        process=subprocess.Popen(argv,stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,
            **({'creationflags':subprocess.CREATE_NO_WINDOW} if os.name=='nt' else {}))
        process.tsfp_stop_path=stop
        try:
            deadline=time.monotonic()+20
            while time.monotonic()<deadline and process.poll() is None and not parent.exited():
                if target.is_file():
                    data=json.loads(target.read_text(encoding='utf-8'))
                    if data.get('error'):raise ValueError(data['error'])
                    pair=(data['directory'],data['fingerprint']);server_invitation(*pair)
                    local=(data.get('local_directory',data['directory']),data['fingerprint'])
                    return process,pair,local
                time.sleep(.05)
            raise ValueError('Private server did not start; check its ports and installation')
        except BaseException:
            stop_private(process)
            raise
    return launch

def run(args,parent):
    result=args.result.resolve()
    request_path=Path(str(result)+'.request');state_path=Path(str(result)+'.state')
    controller=Controller(args.compatibility,private_launcher(result,parent))
    previous=previous_state=previous_players=None
    try:
        while not parent.exited() and not result.with_suffix('.stop').exists():
            try:
                data=request_path.read_bytes()
                if data!=previous:
                    request=decode_request(data);controller.begin(request);previous=data
            except FileNotFoundError:pass
            except (OSError,ValueError) as error:
                with controller.lock:controller.status=ERROR;controller.message=str(error)
            try:controller.update_metadata(Path(str(result)+'.match').read_bytes())
            except (OSError,ValueError):pass # Invalid display details cannot interrupt a session.
            try:
                players=Path(str(result)+'.players').read_bytes()
                if players!=previous_players:
                    controller.update_players(players);previous_players=players
            except (OSError,ValueError):pass
            data=controller.snapshot()
            if data!=previous_state:
                temporary=Path(str(state_path)+'.tmp');temporary.write_bytes(data)
                try:temporary.replace(state_path)
                except PermissionError:pass # Windows reader may briefly deny replacement.
                else:previous_state=data
            time.sleep(.05)
    finally:
        controller.close()
        for path in (result,result.with_suffix('.stop'),request_path,state_path,Path(str(state_path)+'.tmp'),Path(str(result)+'.server'),Path(str(result)+'.match'),Path(str(result)+'.match.tmp'),Path(str(result)+'.players'),Path(str(result)+'.players.tmp')):
            path.unlink(missing_ok=True)
        parent.close()
