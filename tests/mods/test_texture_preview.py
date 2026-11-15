"""Original DDS vectors and private moderation API acceptance (fixture scanner)."""
import base64
import io
import os
from pathlib import Path
import struct
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
from mod_package import ModError, build_package, digest
from mod_texture_preview import thumbnail
from mod_api import WorkshopAPI
from test_creator_api import call
from test_creator_service import service, account, package, Scanner

PATH = 'textures/0123456789abcdef.dds'


def dds(kind='BC1', width=4, height=4, block=None):
    header = bytearray(128)
    values = {0:0x20534444,4:124,12:height,16:width,28:max(width,height).bit_length(),76:32}
    if kind == 'BGRA8':
        values.update({80:0x41,88:32,92:0xff0000,96:0xff00,100:0xff,104:0xff000000})
        block = block or b'\x00\x00\xff\xff'
    else:
        values.update({80:4,84:{'BC1':0x31545844,'BC2':0x33545844,'BC3':0x35545844}[kind]})
        block = block or ((b'\xff'*8 if kind=='BC2' else b'\xff\x00'+b'\x00'*6) if kind!='BC1' else b'') + struct.pack('<HHI',0xf800,0,0)
    for offset,value in values.items():
        struct.pack_into('<I',header,offset,value)
    payload = bytearray()
    for _ in range(values[28]):
        count = width*height if kind=='BGRA8' else ((width+3)//4)*((height+3)//4)
        payload.extend(block*count)
        width,height=max(1,width//2),max(1,height//2)
    return bytes(header+payload)


@pytest.mark.parametrize('kind',['BC1','BC2','BC3','BGRA8'])
def test_supported_formats_have_known_red_pixels_and_exact_bound(kind):
    meta,rgba=thumbnail(dds(kind))
    assert meta==dict(width=4,height=4,format=kind,preview_width=4,preview_height=4)
    assert rgba==b'\xff\x00\x00\xff'*16


@pytest.mark.parametrize('width,height',[(1,4096),(4096,1),(4096,4096),(256,128),(2,4)])
def test_sampling_uses_base_level_with_bounded_aspect(width,height):
    data=bytearray(dds('BC1',width,height))
    base=((width+3)//4)*((height+3)//4)*8
    for offset in range(128+base,len(data),8):
        struct.pack_into('<HHI',data,offset,0x1f,0,0)  # Every smaller mip is blue.
    meta,rgba=thumbnail(data)
    assert 1<=meta['preview_width']<=128 and 1<=meta['preview_height']<=128
    assert len(rgba)==meta['preview_width']*meta['preview_height']*4<=65536
    assert rgba==b'\xff\x00\x00\xff'*(len(rgba)//4)


def test_bc1_transparent_selector_and_bc2_explicit_alpha():
    _,rgba=thumbnail(dds(block=struct.pack('<HHI',0,0xffff,0xffffffff)))
    assert rgba==b'\0'*64
    _,rgba=thumbnail(dds('BC2',block=bytes.fromhex('1032547698badcfe')+struct.pack('<HHI',0xf800,0,0)))
    assert rgba[3::4]==bytes(i*17 for i in range(16))


@pytest.mark.parametrize('kind',['BC1','BC2','BC3'])
def test_color_selectors_and_low_endpoint_order(kind):
    alpha=b'' if kind=='BC1' else (b'\xff'*8 if kind=='BC2' else b'\xff\x00'+b'\0'*6)
    _,rgba=thumbnail(dds(kind,block=alpha+struct.pack('<HHI',0x1f,0xf800,0xe4e4e4e4)))
    row=bytes((0,0,255,255,255,0,0,255))
    row+=bytes((127,0,127,255,0,0,0,0)) if kind=='BC1' else bytes((85,0,170,255,170,0,85,255))
    assert rgba==row*4


@pytest.mark.parametrize('a0,a1,expected',[(255,0,[255,0,218,182,145,109,72,36]),
                                         (0,255,[0,255,51,102,153,204,0,255])])
def test_bc3_all_alpha_selectors(a0,a1,expected):
    selectors=sum((i%8)<<(i*3) for i in range(16))
    block=bytes((a0,a1))+selectors.to_bytes(6,'little')+struct.pack('<HHI',0xf800,0,0)
    _,rgba=thumbnail(dds('BC3',block=block))
    assert rgba[3::4]==bytes(expected*2)


@pytest.mark.parametrize('data',[b'',dds()[:-1],dds()+b'x'])
def test_invalid_dds_rejected_before_sampling(data):
    with pytest.raises(ModError):
        thumbnail(data)


def texture_upload(service,tmp_path):
    accounts,uploads,_=service
    creator,token,_=account(accounts)
    reviewer,reviewer_token,_=account(accounts,'reviewer','moderator')
    package(tmp_path)
    source=tmp_path/'test-addon-1.0.0-alice'
    (source/'textures').mkdir()
    (source/PATH).write_bytes(dds())
    output=tmp_path/'texture.zip'
    build_package(source,output)
    data=output.read_bytes()
    ticket=uploads.begin(creator,'test-addon','1.0.0',digest(data),len(data))
    uploads.receive(creator,ticket['id'],io.BytesIO(data),len(data))
    return accounts,uploads,reviewer,token,reviewer_token,data,ticket


def test_texture_preview_requires_role_completed_scan_and_exact_payload(service,tmp_path):
    accounts,uploads,reviewer,creator_token,token,data,ticket=texture_upload(service,tmp_path)
    api=WorkshopAPI(accounts,'https://workshop.invalid',uploads=uploads)
    route='/v1/moderation/'+ticket['id']+'/texture'
    request=dict(sha256=digest(data),path=PATH)
    assert call(api,'POST',route,request)['status']==401
    assert call(api,'POST',route,request,creator_token)['status']==403
    assert call(api,'POST',route,request,token)['status']==404
    uploads.process_next(Scanner())
    result=call(api,'POST',route,request,token)
    assert result['status']==200
    value=result['body']
    assert value==dict(id=ticket['id'],sha256=digest(data),path=PATH,payload_sha256=digest(dds()),
                      size=len(dds()),width=4,height=4,format='BC1',preview_width=4,preview_height=4,
                      rgba=base64.b64encode(b'\xff\0\0\xff'*16).decode())
    for changes,status in [({'sha256':'f'*64},409),({'path':'../secret.dds'},400),
                           ({'path':'scripts/main.lua'},400),({'path':'textures/ffffffffffffffff.dds'},400)]:
        assert call(api,'POST',route,dict(request,**changes),token)['status']==status
    assert call(api,'POST',route,dict(request,offset=0),token)['status']==400
    assert accounts.repository.catalog()==[]
    blob=accounts.repository.blob(digest(data))
    blob.write_bytes(b'changed')
    assert call(api,'POST',route,request,token)['status']==409


def test_texture_preview_account_denial_does_not_charge_global_limit(service,tmp_path):
    accounts,uploads,reviewer,_,token,data,ticket=texture_upload(service,tmp_path)
    uploads.process_next(Scanner())
    api=WorkshopAPI(accounts,'https://workshop.invalid',uploads=uploads)
    with accounts.repository.db() as db:
        for _ in range(24):
            assert accounts.rate(db,'texture-preview-account',str(reviewer.id),24,60)
    for _ in range(50):
        assert call(api,'POST','/v1/moderation/'+ticket['id']+'/texture',
                    dict(sha256=digest(data),path=PATH),token)['status']==429
    with accounts.repository.db() as db:
        for _ in range(48):
            assert accounts.rate(db,'texture-preview-global','all',48,60)


@pytest.mark.skipif(os.name!='posix',reason='Actual POSIX nofollow/ownership/mode contract')
@pytest.mark.parametrize('damage',['symlink','hardlink','writable','fifo'])
def test_texture_preview_refuses_unsafe_stored_blob(service,tmp_path,damage):
    accounts,uploads,_,_,token,data,ticket=texture_upload(service,tmp_path)
    uploads.process_next(Scanner())
    blob=accounts.repository.blob(digest(data))
    other=tmp_path/'original-fixture.zip'
    if damage=='symlink':
        other.write_bytes(blob.read_bytes());blob.unlink();blob.symlink_to(other)
    elif damage=='hardlink':
        os.link(blob,other)
    elif damage=='writable':
        blob.chmod(0o666)
    else:
        blob.unlink();os.mkfifo(blob,0o600)
    api=WorkshopAPI(accounts,'https://workshop.invalid',uploads=uploads)
    result=call(api,'POST','/v1/moderation/'+ticket['id']+'/texture',dict(sha256=digest(data),path=PATH),token)
    assert result['status']==503 and 'rgba' not in result['body']
