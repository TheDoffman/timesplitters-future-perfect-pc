"""Original native Win32 i386 reference for raise and nested dispatch semantics."""
from pathlib import Path
import os,re,subprocess
import pytest
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]


@pytest.mark.skipif(os.name!='nt',reason='actual Win32 x86 exception oracle')
def test_native_raise_context_edits_nested_flags_and_live_chain(tmp_path):
    cc,env=native_compiler(tmp_path);cc=Path(cc).parent.parent/'x86/cl.exe';assert cc.is_file()
    env=dict(env);env['PATH']=str(cc.parent)+os.pathsep+env['PATH']
    env['LIB']=re.sub(r'(?i)([\\/])x64(?=;|$)',r'\1x86',env['LIB']);env['VSCMD_ARG_TGT_ARCH']='x86'
    source=tmp_path/'raise_x86_contract.c';source.write_bytes((ROOT/'tests/compiler'/source.name).read_bytes())
    exe=tmp_path/'raise.exe'
    r=subprocess.run([str(cc),'/nologo','/Od','/Oy-','/GS-','/W4','/WX',str(source),
        '/Fe:'+str(exe),'/link','/SAFESEH:NO'],env=env,cwd=tmp_path,capture_output=True,text=True,timeout=60)
    assert r.returncode==0,r.stdout+r.stderr
    r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    assert r.returncode==0,r.stdout+r.stderr
    case=None;cases=[];counts={}
    for line in r.stdout.splitlines():
        m=re.fullmatch(r'case(\d+) route(\d+) eax([0-9a-f]+) sp(\d+) bp(\d+) head(\d+) handlers(\d+) flags([0-9a-f]+) address(\d+) params(\d+)',line)
        if m:
            case=int(m[1]);cases.append(case);counts[case]=0
            assert int(m[2])==(2 if case&2 else 1),line
            assert int(m[3],16)==(0x87654321 if case&2 else 0xdeadbeef),line
            assert m[4]==m[5]==m[6]==m[9]==m[10]=='1',line
            assert int(m[7])==(2 if case&5 else 1) and int(m[8],16)==0,line
            continue
        m=re.fullmatch(r'handler(\d+) code([0-9a-f]+) flags([0-9a-f]+) ctxflags([0-9a-f]+) ordinary(\d+) sp(\d+) bp(\d+) eax([0-9a-f]+) ebx([0-9a-f]+) ecx([0-9a-f]+) edx([0-9a-f]+) dispatcher([0-9a-f]+) wrapped(\d+) address(\d+) edited(\d+)',line)
        assert m and case is not None,line
        index=int(m[1]);assert index==counts[case];counts[case]+=1
        assert int(m[2],16)==0xe0424771,line
        assert int(m[3],16)==(16 if case&4 and index else 0),line
        # Newer Windows may include extended/XSTATE groups; this is not an Xbox flag oracle.
        assert int(m[4],16)&0x10003==0x10003,line
        edited=bool(case&2 and index)
        assert int(m[5])==int(not edited) and m[6]==m[7]=='1',line
        assert int(m[8],16)==(0x87654321 if edited else 0xdeadbeef),line
        assert [int(m[n],16) for n in (9,10,11)]==[0x11223344,0x55667788,0x99aabbcc],line
        assert int(m[12],16) and int(m[13])==int(not index) and m[14]=='1',line
        assert int(m[15])==int(edited),line
    assert cases==list(range(8))
    assert all(counts[n]==(2 if n&5 else 1) for n in cases)
