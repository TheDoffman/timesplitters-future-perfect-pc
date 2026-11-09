import os
import subprocess
from pathlib import Path
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


def test_graphics_options(tmp_path):
    compiler, env = native_compiler(tmp_path)
    source = ROOT / 'tests/graphics/graphics_options.c'
    exe = tmp_path / ('graphics.exe' if os.name == 'nt' else 'graphics')
    args = ([compiler, '/nologo', '/std:c11', '/W4', '/WX',
             '/D_CRT_SECURE_NO_WARNINGS', '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
            if os.name == 'nt' else
            [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-I' + str(ROOT / 'src'),
             str(source), '-o', str(exe)])
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], cwd=tmp_path, capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr


def test_legacy_graphics_preferences(tmp_path):
    """Link the GL preference adapter without a GPU; preserve Vulkan choices."""
    compiler, env = native_compiler(tmp_path)
    source = tmp_path / 'legacy.c'
    source.write_text(r"""
#include <assert.h>
#include "native/graphics_preferences_gl.inc"
int recomp_config_int(const char *env,const char *key,int fallback){
    (void)env;(void)key;(void)fallback;return 8;
}
uint32_t recomp_config_title_id(void){return 0x45410066;}
int recomp_config_user_dir(char *out,size_t n){return snprintf(out,n,".")>0;}
int main(void){
    TsfpGraphicsOptions o=pc_graphics_get();assert(o.anisotropy==8);
    o.msaa=8;o.edge_smoothing=1;o.anisotropy=16;o.ambient_occlusion=1;
    assert(pc_graphics_set(&o));
    TsfpGraphicsOptions saved=tsfp_graphics_defaults();
    FILE *file=fopen("pc-graphics.ini","rb");assert(file);
    assert(tsfp_graphics_read(file,&saved));fclose(file);
    assert(saved.msaa==8 && saved.edge_smoothing==1 && saved.anisotropy==16 && saved.ambient_occlusion==1);
    assert(pc_graphics_clear_cache()==0);
    return 0;
}
""", encoding='utf-8')
    exe=tmp_path/('legacy.exe' if os.name=='nt' else 'legacy')
    includes=[ROOT/'src',ROOT/'external/ts3FPrecomp/src/config']
    args=([compiler,'/nologo','/std:c11','/D_CRT_SECURE_NO_WARNINGS',
           *['/I'+str(p) for p in includes],str(source),'/Fe:'+str(exe)] if os.name=='nt' else
          [compiler,'-std=c11','-D_POSIX_C_SOURCE=200809L','-Wall','-Wextra','-Werror',
           *['-I'+str(p) for p in includes],str(source),'-o',str(exe)])
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    run_env=dict(env,TSFP_DISPLAY_CONFIG=str(tmp_path/'display.ini'))
    result=subprocess.run([str(exe)],cwd=tmp_path,env=run_env,capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
