"""Player-side identity metadata: bounded, host-only, optional display reporting."""
import os
from pathlib import Path
import struct
import subprocess
from types import SimpleNamespace

import pytest
from native_compiler import native_compiler
import online_menu as menu
from online_arcade_client import NativeArcade

ROOT=Path(__file__).resolve().parents[2]


def roster():
    fields=menu.PERSON.pack(0,72,-1,528,menu.field('Partner',32),menu.field('Character',48))
    return menu.ROSTER_HEADER.pack(0x31524f54,1,53,120,9,1,1)+fields+bytes(15*menu.PERSON.size)


def test_native_capture_and_python_decode_agree(tmp_path):
    compiler,environment=native_compiler(tmp_path)
    exe=tmp_path/('roster.exe' if os.name=='nt' else 'roster')
    source=ROOT/'tests/network/online_roster.c'
    if Path(compiler).name.lower() in ('cl','cl.exe'):
        args=[compiler,'/nologo','/std:c11','/W4','/I'+str(ROOT/'src'),str(source),'/Fe:'+str(exe)]
    else:args=[compiler,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),str(source),'-o',str(exe)]
    result=subprocess.run(args,cwd=tmp_path,env=environment,text=True,capture_output=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    output=tmp_path/'roster.bin'
    result=subprocess.run([str(exe),str(output)],text=True,capture_output=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
    value=menu.decode_roster(output.read_bytes())
    assert [p['slot'] for p in value['people']]==[0,2]
    assert value['people'][1]['name']=='Playername'
    assert value['people'][0]['portrait']==528 and value['scene']==53


@pytest.mark.parametrize('offset,value',[(0,0),(4,2),(8,101),(16,256),(20,17),(24,0),(28,16),(32,4096),(36,128),(40,1024)])
def test_malformed_identity_reports_rejected(offset,value):
    data=bytearray(roster());struct.pack_into('<I',data,offset,value)
    with pytest.raises(ValueError):menu.decode_roster(data)


def test_only_current_host_publishes_and_old_identity_data_expires():
    clock=[100.]
    bridge=SimpleNamespace(peer_id=7,clock=lambda:clock[0])
    native=NativeArcade(bridge);native.status=dict(epoch=1,host_peer=7,phase='active')
    controller=menu.Controller('a'*64);controller.session=SimpleNamespace(native=native);controller.mode=1
    controller.update_players(roster())
    assert native.body('a'*64)['player_info']['people'][0]['name']=='Partner'
    native.status['host_peer']=8
    assert 'player_info' not in native.body('a'*64)
    native.roster_info=None;controller.update_players(roster());assert native.roster_info is None
    native.status['host_peer']=7;controller.update_players(roster());clock[0]+=6
    assert 'player_info' not in native.body('a'*64)
