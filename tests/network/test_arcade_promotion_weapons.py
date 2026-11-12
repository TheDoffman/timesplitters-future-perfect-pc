import os,subprocess
from pathlib import Path
import pytest
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]
@pytest.mark.parametrize('variant',['windows','native','fallback'])
def test_owned_replica_weapons_are_retired_on_promotion(tmp_path,variant):
 compiler,env=native_compiler(tmp_path)
 production=(ROOT/'src/network/pc_arcade_weapons.inc').read_text(encoding='utf-8')
 source=(ROOT/'tests/network/arcade_promotion_weapons.c').read_text(encoding='utf-8').replace('/* PRODUCTION */',production)
 # System headers retain their platform defines; select the production branch afterwards.
 marker='static unsigned char ram'
 defines={'windows':'#define _WIN32 1','native':'#undef _WIN32\n#define TSFP_NATIVE_NETWORK 1','fallback':'#undef _WIN32\n#define EXPECT_FALLBACK 1'}[variant]
 source=source.replace(marker,defines+'\n'+marker)
 path=tmp_path/'promotion.c';path.write_text(source,encoding='utf-8');exe=tmp_path/('promotion.exe' if os.name=='nt' else 'promotion')
 if Path(compiler).name.lower() in ('cl','cl.exe'):args=[compiler,'/nologo','/std:c11','/W4','/WX',str(path),'/Fe:'+str(exe)]
 else:args=[compiler,'-std=c11','-Wall','-Wextra','-Werror',str(path),'-o',str(exe)]
 result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60);assert result.returncode==0,result.stdout+result.stderr
 result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10);assert result.returncode==0,result.stdout+result.stderr
 assert 'PASS:' in result.stdout
