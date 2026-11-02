import os
import subprocess
from pathlib import Path
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


def test_persistent_shader_cache(tmp_path):
    compiler, env = native_compiler(tmp_path)
    runtime = ROOT/'external/ts3FPrecomp/src'
    exe = tmp_path/('cache.exe' if os.name == 'nt' else 'cache')
    sources = [ROOT/'tests/graphics/shader_cache.c', runtime/'d3d/rhi_shader_cache.c']
    includes = [ROOT/'src', runtime/'d3d', runtime/'config']
    args = ([compiler, '/nologo', '/std:c11', '/D_CRT_SECURE_NO_WARNINGS',
             *['/I'+str(p) for p in includes], *map(str,sources), '/Fe:'+str(exe)]
            if os.name == 'nt' else
            [compiler, '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-Wall', '-Wextra', '-Werror',
             *['-I'+str(p) for p in includes], *map(str,sources), '-o', str(exe)])
    result = subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode == 0, result.stdout+result.stderr
    env = dict(env, RECOMP_SHADER_CACHE='1', TSFP_DISPLAY_CONFIG=str(tmp_path/'display.ini'))
    root = tmp_path/'nested'/'user'
    def run(action, backend='fixture-v1', expected=0):
        result = subprocess.run([str(exe),str(root),action,backend,str(expected)],
                                cwd=tmp_path,env=env,capture_output=True,text=True,timeout=10)
        assert result.returncode == 0, result.stdout+result.stderr
    run('get');run('put');run('get',expected=1)
    run('get','fixture-v2')  # Backend/compiler ABI changes invalidate the key.
    entry = next(root.rglob('*.bin'))
    good = entry.read_bytes()
    for damaged in (good[:-1], good+b'junk', good[:-1]+bytes([good[-1]^1]), b'bad',
                    good[:4]+(1).to_bytes(4,'little')+good[8:]):
        entry.write_bytes(damaged);run('get')
        run('put');run('get',expected=1)  # A fresh compile repairs the entry.
    run('path')
    preferences = tmp_path/'pc-graphics.ini'
    preferences.write_text('TSFP_GRAPHICS_V1 1 0 1 0 0\n')
    run('get')
    preferences.write_text('TSFP_GRAPHICS_V1 1 0 1 0 1\n')
    run('get',expected=1)
    env['RECOMP_SHADER_CACHE']='0';run('get')
    run('put','disabled');assert not (root/'shadercache'/'45410066'/'disabled').exists()
    (entry.parent/'keep.bin').write_bytes(b'unrelated')
    (entry.parent/'nested').mkdir()
    nested = entry.parent/'nested'/entry.name
    nested.write_bytes(b'unrelated nested')
    run('clear', expected=1)
    assert not entry.exists()
    assert (entry.parent/'keep.bin').read_bytes() == b'unrelated'
    assert nested.read_bytes() == b'unrelated nested'
    run('clear', expected=0)


def test_pipeline_cache_integrity(tmp_path):
    """Execute Vulkan's real loader without requiring a Vulkan device."""
    compiler,env=native_compiler(tmp_path)
    runtime=(ROOT/'external/ts3FPrecomp/src/d3d/rhi_vulkan.c').read_text(encoding='utf-8')
    start=runtime.index('static void *pcache_load(')
    loader=runtime[start:runtime.index('static void pcache_save(',start)]
    code=r"""
#include <assert.h>
#include "graphics/pipeline_cache_file.h"
#define VK_PIPELINE_CACHE_HEADER_VERSION_ONE 1
#define VK_UUID_SIZE 16
#define PIPELINE_CACHE_DIR "vulkan-pipelines"
#define PIPELINE_CACHE_FILE "pipelines.bin"
static struct {struct {uint32_t vendorID,deviceID;unsigned char pipelineCacheUUID[16];} props;} V;
static int rhi_cache_file_path(const char *backend,const char *name,char *out,size_t n){
 (void)backend;(void)name;snprintf(out,n,"pipeline.bin");return 1;
}
/* LOADER */
static void save(const void *data,size_t n){
 FILE *f=fopen("pipeline.bin","wb");assert(f);
 assert(tsfp_pipeline_file_write(f,data,n));assert(!fclose(f));
}
static void load(int expected){
 size_t n=999;void *data=pcache_load(&n);
 assert((data!=NULL)==expected);assert(n==(expected?64:0));free(data);
}
int main(void){
 unsigned char data[64]={0},file[88];uint32_t h[4]={32,1,7,8};
 V.props.vendorID=7;V.props.deviceID=8;
 memcpy(data,h,16);memset(data+32,42,32);
 save(data,64);load(1);
 V.props.vendorID=9;load(0);V.props.vendorID=7;
 V.props.deviceID=9;load(0);V.props.deviceID=8;
 V.props.pipelineCacheUUID[0]=1;load(0);V.props.pipelineCacheUUID[0]=0;
 h[0]=100;memcpy(data,h,16);save(data,64);load(0);
 h[0]=32;memcpy(data,h,16);save(data,64);load(1);
 FILE *f=fopen("pipeline.bin","rb");assert(fread(file,1,88,f)==88);fclose(f);
 for(size_t n=0;n<88;n++){
   f=fopen("pipeline.bin","wb");fwrite(file,1,n,f);fclose(f);load(0);
 }
 for(size_t i=0;i<88;i++){
   file[i]^=1;f=fopen("pipeline.bin","wb");fwrite(file,1,88,f);fclose(f);load(0);file[i]^=1;
 }
 f=fopen("pipeline.bin","wb");fwrite(file,1,88,f);fputc(0,f);fclose(f);load(0);
 /* Oversized advertised allocations are rejected before allocation. */
 tsfp_pipeline_u64(file+8,UINT64_MAX);
 f=fopen("pipeline.bin","wb");fwrite(file,1,88,f);fclose(f);load(0);
 /* Legacy opaque blobs have no integrity envelope and must rebuild. */
 f=fopen("pipeline.bin","wb");fwrite(data,1,64,f);fclose(f);load(0);
 save(data,64);load(1);
 return 0;
}
""".replace('/* LOADER */',loader)
    source=tmp_path/'pipeline.c';source.write_text(code,encoding='utf-8')
    exe=tmp_path/('pipeline.exe' if os.name=='nt' else 'pipeline')
    args=([compiler,'/nologo','/std:c11','/D_CRT_SECURE_NO_WARNINGS','/I'+str(ROOT/'src'),str(source),'/Fe:'+str(exe)]
          if os.name=='nt' else [compiler,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),str(source),'-o',str(exe)])
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
