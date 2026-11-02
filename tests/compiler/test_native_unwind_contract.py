"""Native x86 evidence for guest context/unwind implementation."""
from pathlib import Path
import os,re,subprocess
import pytest
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]
@pytest.mark.skipif(os.name!='nt',reason='actual Win32 x86 runtime oracle')
def test_native_unwind_context_edits_and_handler_chain(tmp_path):
    cc,env=native_compiler(tmp_path);cc=Path(cc).parent.parent/'x86/cl.exe';assert cc.is_file()
    env=dict(env);env['PATH']=str(cc.parent)+os.pathsep+env['PATH'];env['LIB']=re.sub(r'(?i)([\\/])x64(?=;|$)',r'\1x86',env['LIB']);env['VSCMD_ARG_TGT_ARCH']='x86'
    p=tmp_path/'unwind_x86_contract.c';p.write_bytes((ROOT/'tests/compiler'/p.name).read_bytes());exe=tmp_path/'unwind.exe'
    r=subprocess.run([str(cc),'/nologo','/Od','/Oy-','/GS-','/W4','/WX',str(p),'/Fe:'+str(exe),'/link','/SAFESEH:NO'],env=env,cwd=tmp_path,capture_output=True,text=True,timeout=60);assert r.returncode==0,r.stdout+r.stderr
    r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20);assert r.returncode==0,r.stdout+r.stderr
    case=None;seen={};cases=[]
    for line in r.stdout.splitlines():
        m=re.fullmatch(r'case(\d+) route(\d+) eax([0-9a-f]+) sp_equal(\d+) bp_equal(\d+) head_target(\d+) handlers(\d+) inputflags([0-9a-f]+)',line)
        if m:
            case=int(m[1]);cases.append(case);seen[case]=[]
            assert int(m[2])==(2 if case&8 else 1),line
            assert int(m[3],16)==(0x87654321 if case&8 else 0x12345678),line
            assert m[4]==m[5]==m[6]=='1',line
            assert int(m[7])==(2 if case&1 else 1),line
            assert int(m[8],16)==(3 if case&4 else 1),line
            continue
        m=re.fullmatch(r'handler(\d+) code([0-9a-f]+) flags([0-9a-f]+) frame_index(\d+) contextflags([0-9a-f]+) context_eax([0-9a-f]+) dispatcher([0-9a-f]+) head_is_frame(\d+) head_next_frame(\d+)',line)
        assert m and case is not None,line
        index=int(m[1]);seen[case].append(index)
        assert int(m[2],16)==(0xe1234567 if case&4 else 0xc0000027),line
        assert int(m[3],16)==(3 if case&4 else 2),line
        assert int(m[4])==index and int(m[5],16)==0x10007,line
        assert int(m[6],16)==(0x87654321 if case&8 and index else 0x12345678),line
        assert int(m[7],16) and m[8]=='0' and m[9]=='1',line
    assert cases==list(range(16))
    assert all(rows==list(range(2 if case&1 else 1)) for case,rows in seen.items())
