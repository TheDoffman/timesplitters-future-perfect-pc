"""Execute the production AO queue adapter with mutable guest camera snapshots."""
import os
from pathlib import Path
import subprocess
from native_compiler import native_compiler
ROOT = Path(__file__).resolve().parents[2]

def test_ambient_occlusion_queue(tmp_path):
    record = (ROOT / 'external/ts3FPrecomp/src/hle/hle_d3d8_record.c').read_text(encoding='utf-8')
    start = record.index('extern void xbox_D3D8AmbientOcclusion(')
    adapter = record[start:record.index('static void op_viewport(', start)]
    harness = r"""
#include <assert.h>
#include <stddef.h>
#include <string.h>
static int recording,hle_d3d8_interp_rec,calls,queued,replays,g_cap,captures;
#define D3D8CAP_AMBIENT_OCCLUSION 32
static float captured[4][16];
static void chunk(int kind,const void *p,size_t n,const void *b,size_t bn,const void *c,size_t cn){
 assert(kind==32 && n==64 && !b && !bn && !c && !cn);
 memcpy(captured[captures++],p,n);
}
static float seen[4][16],snapshot[4][16],replay[16];
static void (*operation[4])(const void *);
void host_AmbientOcclusion(const float projection[16]);
int hle_d3d8_defer_recording(void){return recording;}
void hle_d3d8_defer_op(void (*fn)(const void *),const void *arg,size_t size){
 assert(size==sizeof snapshot[0]);operation[queued]=fn;memcpy(snapshot[queued++],arg,size);
}
void hle_d3d8_interp_op(void (*fn)(const void *),const void *arg,size_t size){
 (void)fn;assert(size==sizeof replay);memcpy(replay,arg,size);replays++;
}
void xbox_D3D8AmbientOcclusion(const float *p){memcpy(seen[calls++],p,sizeof seen[0]);}
/* ADAPTER */
int main(void){
 float p[16];for(int i=0;i<16;i++)p[i]=(float)i;
 g_cap=1;recording=1;host_AmbientOcclusion(p);p[0]=42;host_AmbientOcclusion(p);p[0]=99;
 assert(!calls&&queued==2&&!captures);recording=0;
 for(int i=0;i<queued;i++)operation[i](snapshot[i]);
 assert(calls==2&&seen[0][0]==0&&seen[1][0]==42);
 hle_d3d8_interp_rec=1;host_AmbientOcclusion(p);p[0]=100;
 assert(calls==3&&replays==1&&seen[2][0]==99&&replay[0]==99);
 host_AmbientOcclusion(NULL);assert(calls==3&&captures==3);
 assert(captured[0][0]==0&&captured[1][0]==42&&captured[2][0]==99);
 g_cap=0;host_AmbientOcclusion(p);assert(calls==4&&captures==3);
 return 0;
}
""".replace('/* ADAPTER */', adapter)
    source=tmp_path/'ao-queue.c';source.write_text(harness,encoding='utf-8')
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path/('queue.exe' if os.name=='nt' else 'queue')
    args=[compiler,'/nologo','/std:c11',str(source),'/Fe:'+str(exe)] if os.name=='nt' else [compiler,'-std=c11',str(source),'-o',str(exe)]
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr


def test_ambient_occlusion_replay_payload(tmp_path):
    source = (ROOT / 'external/ts3FPrecomp/src/replay/d3d8_replay.c').read_text(encoding='utf-8')
    start = source.index('    case D3D8CAP_AMBIENT_OCCLUSION: {')
    adapter = source[start:source.index('    case D3D8CAP_TWOD_PLACEMENT:', start)]
    harness = r"""
#include <assert.h>
#include <stddef.h>
#include <string.h>
#define D3D8CAP_AMBIENT_OCCLUSION 32
static int calls;
static float seen[16];
void xbox_D3D8AmbientOcclusion(const float *p){calls++;memcpy(seen,p,sizeof seen);}
static unsigned run(const void *data,size_t bytes){
 struct {unsigned malformed;} state={0},*r=&state;
 struct {const void *data;size_t bytes;} command={data,bytes},*c=&command;
 switch(32){
/* ADAPTER */
 }
 return r->malformed;
}
int main(void){
 float p[16];unsigned char unaligned[66];
 for(int i=0;i<16;i++)p[i]=(float)(i+3);
 memcpy(unaligned+1,p,sizeof p);
 assert(run(unaligned+1,64)==0&&calls==1&&!memcmp(p,seen,64));
 for(size_t n=0;n<64;n++)assert(run(unaligned,n)==1);
 assert(run(unaligned,65)==1&&calls==1);
 return 0;
}
""".replace('/* ADAPTER */', adapter)
    file=tmp_path/'ao-replay.c';file.write_text(harness,encoding='utf-8')
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path/('replay.exe' if os.name=='nt' else 'replay')
    args=[compiler,'/nologo','/std:c11',str(file),'/Fe:'+str(exe)] if os.name=='nt' else [compiler,'-std=c11',str(file),'-o',str(exe)]
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
