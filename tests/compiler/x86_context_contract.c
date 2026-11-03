#include "guest_x86_context.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
#ifdef TSFP_NATIVE_CONTEXT_ORACLE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
_Static_assert(sizeof(CONTEXT)==716,"Win32 i386 CONTEXT required");
static int sdk_encode(void *buffer,size_t bytes,const tsfp_x86_context_state *s,uint32_t flags) {
 CONTEXT c;if(bytes<716 || (flags&0xffffffc0u)!=0x10000)return 0;memcpy(&c,buffer,716);c.ContextFlags=flags;
 if(flags&1) {c.Ebp=s->r_ebp;c.Eip=s->r_eip;c.SegCs=s->cs;c.EFlags=s->r_eflags;c.Esp=s->r_esp;c.SegSs=s->ss;}
 if(flags&2) {c.Edi=s->r_edi;c.Esi=s->r_esi;c.Ebx=s->r_ebx;c.Edx=s->r_edx;c.Ecx=s->r_ecx;c.Eax=s->r_eax;}
 if(flags&4) {c.SegGs=s->gs;c.SegFs=s->fs;c.SegEs=s->es;c.SegDs=s->ds;}
 if(flags&16) {c.Dr0=s->debug[0];c.Dr1=s->debug[1];c.Dr2=s->debug[2];c.Dr3=s->debug[3];c.Dr6=s->debug[4];c.Dr7=s->debug[5];}
 if(flags&8)memcpy(&c.FloatSave,s->x87,sizeof c.FloatSave);
 if(flags&32)memcpy(c.ExtendedRegisters,s->extended,sizeof c.ExtendedRegisters);
 memcpy(buffer,&c,716);return 1;
}
static uint32_t sdk_decode(tsfp_x86_context_state *s,const void *buffer,size_t bytes) {
 CONTEXT c;if(bytes<716)return 0;memcpy(&c,buffer,716);
 if((c.ContextFlags&0xffffffc0u)!=0x10000)return 0;
 if(c.ContextFlags&1) {s->r_ebp=c.Ebp;s->r_eip=c.Eip;s->cs=c.SegCs;s->r_eflags=c.EFlags;s->r_esp=c.Esp;s->ss=c.SegSs;}
 if(c.ContextFlags&2) {s->r_edi=c.Edi;s->r_esi=c.Esi;s->r_ebx=c.Ebx;s->r_edx=c.Edx;s->r_ecx=c.Ecx;s->r_eax=c.Eax;}
 if(c.ContextFlags&4) {s->gs=c.SegGs;s->fs=c.SegFs;s->es=c.SegEs;s->ds=c.SegDs;}
 if(c.ContextFlags&16) {s->debug[0]=c.Dr0;s->debug[1]=c.Dr1;s->debug[2]=c.Dr2;s->debug[3]=c.Dr3;s->debug[4]=c.Dr6;s->debug[5]=c.Dr7;}
 if(c.ContextFlags&8)memcpy(s->x87,&c.FloatSave,sizeof c.FloatSave);
 if(c.ContextFlags&32)memcpy(s->extended,c.ExtendedRegisters,sizeof c.ExtendedRegisters);
 return c.ContextFlags;
}
#define ENCODE sdk_encode
#define DECODE sdk_decode
#else
#define ENCODE tsfp_x86_context_encode
#define DECODE tsfp_x86_context_decode
#endif
#define CHECK(x) do {if(!(x)){fprintf(stderr,"context line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int aliases(void) {
 unsigned char *arena=malloc(2048),expected[2048],record[716];
 tsfp_x86_context_state initial={0},snapshot;int shifts[]={-128,-4,0,4,32,128};
 unsigned group,i,j,mode;CHECK(arena);
 for(i=0;i<sizeof initial;i++)((unsigned char *)&initial)[i]=(unsigned char)(i*37+11);
 for(group=0;group<64;group++)for(j=0;j<sizeof shifts/sizeof shifts[0];j++)for(mode=0;mode<2;mode++) {
  unsigned at=(unsigned)(256+shifts[j]);uint32_t flags=0x10000|group;
  tsfp_x86_context_state *state=(tsfp_x86_context_state *)(arena+256);
  for(i=0;i<2048;i++)arena[i]=(unsigned char)(i*73+19);
  memcpy(state,&initial,sizeof initial);
  if(mode) {arena[at]=(unsigned char)flags;arena[at+1]=(unsigned char)(flags>>8);arena[at+2]=1;arena[at+3]=0;}
  memcpy(&snapshot,state,sizeof snapshot);memcpy(record,arena+at,716);memcpy(expected,arena,2048);
  if(mode) {
   CHECK(tsfp_x86_context_decode(&snapshot,record,716)==flags);
   memcpy(expected+256,&snapshot,sizeof snapshot);
   CHECK(tsfp_x86_context_decode(state,arena+at,716)==flags);
  }else {
   CHECK(tsfp_x86_context_encode(record,716,&snapshot,flags));memcpy(expected+at,record,716);
   CHECK(tsfp_x86_context_encode(arena+at,716,state,flags));
  }
  CHECK(!memcmp(arena,expected,2048));
 }
 memcpy(&snapshot,&initial,sizeof snapshot);memset(record,0x59,sizeof record);
 CHECK(!tsfp_x86_context_encode(NULL,716,&initial,0x1003f));
 CHECK(!tsfp_x86_context_encode(record,716,NULL,0x1003f));
 CHECK(!tsfp_x86_context_decode(NULL,record,716));
 CHECK(!tsfp_x86_context_decode(&initial,NULL,716));CHECK(!memcmp(&snapshot,&initial,sizeof snapshot));
 for(i=0;i<716;i++)CHECK(record[i]==0x59);
 free(arena);return 0;
}
int main(void) {
 uint32_t q[3],result;tsfp_x86_context_state state;unsigned char record[716];
 _Static_assert(sizeof state==712,"fixture wire layout");
 CHECK(!aliases());
#ifdef _WIN32
 _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
#endif
 while(fread(q,sizeof q,1,stdin)==1) {
  CHECK(fread(&state,sizeof state,1,stdin)==1);CHECK(fread(record,716,1,stdin)==1);
  result=q[0]?DECODE(&state,record,q[2]):(uint32_t)ENCODE(record,q[2],&state,q[1]);
  CHECK(fwrite(&result,4,1,stdout)==1);CHECK(fwrite(&state,sizeof state,1,stdout)==1);CHECK(fwrite(record,716,1,stdout)==1);
 }
 return ferror(stdin)?2:0;
}
