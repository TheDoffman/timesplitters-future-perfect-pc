#include "guest_context_execution.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif
#define CHECK(x) do {if(!(x)){fprintf(stderr,"execution line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct machine {
 tsfp_x86_context_state cpu,expected;
 unsigned expected_set,expected_scopes,inner_cleaned;
 struct machine *cleanup_parent;
 tsfp_context_execution_ops ops;
 unsigned mode,steps,applies,scopes,rewinds,old_after,outer_after,target_seen;
 int cleanup_fail,force_foreign,adapter_returns;
#ifdef _WIN32
 void *fiber_a,*fiber_b;jmp_buf *pending;int jump_code;unsigned adapter_calls;
#endif
} machine;
static int allow_pc(void *opaque,uint32_t pc) {(void)opaque;return pc==0x1000 || pc==0x1100 || pc==0x2000 || pc==0x2500 || pc==0x3000;}
static int allow_state(void *opaque,const tsfp_x86_context_state *state) {(void)opaque;return state->cs==8 && !(state->r_eflags&0x20000);}
static void apply(void *opaque,const tsfp_x86_context_state *state) {
 machine *m=opaque;++m->applies;
 if(m->expected_set && state->r_eip==m->expected.r_eip) {CHECK(!memcmp(state,&m->expected,sizeof *state));CHECK(m->scopes==m->expected_scopes);}
 m->cpu=*state;
}
static uint64_t mark(void *opaque) {return ((machine *)opaque)->scopes;}
static int rewind_scopes(void *opaque,uint64_t n) {
 machine *m=opaque;++m->rewinds;if(m->cleanup_parent)++m->cleanup_parent->inner_cleaned;CHECK(n<=m->scopes);m->scopes=(unsigned)n;return !m->cleanup_fail;
}
static int same_stack(void *opaque,uintptr_t tag) {
 machine *m=opaque;CHECK(tag);
 if(m->force_foreign)return 0;
#ifdef _WIN32
 {ULONG_PTR low,high;GetCurrentThreadStackLimits(&low,&high);return tag>=low && tag<high;}
#else
 return 1; /* These portable cases never switch native stacks. */
#endif
}
static void transfer_target(machine *m,uint32_t pc,uint32_t sp) {
 unsigned i;
 tsfp_x86_context_state changed=m->cpu;changed.r_eip=pc;changed.r_esp=sp;changed.r_eax=0x87654321;
 changed.r_ebx=0x11223344;changed.r_ecx=0x55667788;changed.r_edx=0x99aabbcc;changed.r_esi=0xfedcba98;changed.r_edi=0x76543210;changed.r_ebp=0x4321;
 for(i=0;i<6;i++)changed.debug[i]=0x12345678u*(i+1);
 for(i=0;i<sizeof changed.x87;i++)changed.x87[i]=(unsigned char)(i*37+17);
 for(i=0;i<sizeof changed.extended;i++)changed.extended[i]=(unsigned char)(i*73+29);
 if(m->cleanup_parent){m->cleanup_parent->expected=changed;m->cleanup_parent->expected_set=1;m->cleanup_parent->expected_scopes=0;}
 m->expected=changed;m->expected_set=1;m->expected_scopes=(m->mode==3 || m->mode==10)?1:0;
 CHECK(tsfp_context_transfer(&changed)==0);++m->old_after;
}
static void init(machine *,unsigned);
static int step(void *opaque,uint32_t pc,uint32_t *next) {
 machine *m=opaque;++m->steps;CHECK(tsfp_context_execution_depth()>0);
 if(pc==0x3000) {CHECK(m->cpu.r_eax==0x87654321);++m->target_seen;*next=0;return 2;}
 if(pc==0x2500) {CHECK(m->cpu.r_eax==0x87654321 && m->scopes==1);++m->target_seen;*next=0;return 2;}
 if(m->mode==16) {*next=0;return 1;}
 if(m->mode==17) {*next=0;return 3;}
 if(m->mode==18) {
  if(pc==0x1000){++m->scopes;transfer_target(m,0x2000,800);CHECK(0);}
  CHECK(pc==0x2000 && m->cpu.r_eax==0x87654321);++m->target_seen;*next=0x2000;return 2;
 }
 if(m->mode==19) {CHECK(pc==0x1000);*next=0x1000;return 2;}
 if(m->mode==20 || m->mode==21) {
  if(pc==0x1000){*next=0x2000;return m->mode==20?1:2;}
  CHECK(pc==0x2000);++m->target_seen;*next=m->mode==20?0x2000:0;return 2;
 }
 if(m->mode==1 || m->mode==11) {
  ++m->scopes;transfer_target(m,0x3000,m->mode==11?1500:800);CHECK(0);
 }
 if(m->mode==2 || m->mode==3) {
  if(pc==0x1000) {
   tsfp_x86_context_state child=m->cpu;child.r_eip=0x2000;child.r_esp=600;++m->scopes;
   CHECK(tsfp_context_execute(&m->ops,&child,100,2000,0)==1);
   ++m->outer_after;CHECK(m->mode==3 && tsfp_context_execution_depth()==1);--m->scopes;*next=0;return 2;
  }
  CHECK(pc==0x2000);++m->scopes;
  transfer_target(m,m->mode==2?0x3000:0x2500,m->mode==2?800:500);CHECK(0);
 }
 if(m->mode==10) {
  if(pc==0x1000 || pc==0x2000) {
   tsfp_x86_context_state child=m->cpu;child.r_eip=pc==0x1000?0x2000:0x1100;child.r_esp=pc==0x1000?800:600;
   ++m->scopes;CHECK(tsfp_context_execute(&m->ops,&child,100,2000,0)==1);
   CHECK(pc==0x1000 && tsfp_context_execution_depth()==1);++m->outer_after;--m->scopes;*next=0;return 2;
  }
  CHECK(pc==0x1100);++m->scopes;transfer_target(m,0x3000,700);CHECK(0);
 }
 if(m->mode==13 || m->mode==15) {
  machine child;CHECK(pc==0x1000);++m->scopes;init(&child,14);
  child.cpu.r_eip=0x2000;child.cpu.r_esp=600;child.cleanup_parent=m;child.cleanup_fail=m->mode==15;
  (void)tsfp_context_execute(&child.ops,&child.cpu,100,2000,0);CHECK(0);
 }
 if(m->mode==14) {
  ++m->scopes;transfer_target(m,0x3000,800);CHECK(0);
 }
 if(m->mode==4) {
  tsfp_x86_context_state child=m->cpu;child.r_eip=0x1000;child.r_esp-=4;
  if(tsfp_context_execution_depth()==64) {
   CHECK(tsfp_context_execute(&m->ops,&child,0,4096,0)==0);CHECK(tsfp_context_execution_depth()==64);++m->target_seen;
  }else CHECK(tsfp_context_execute(&m->ops,&child,0,4096,0)==1);
  *next=0;return 2;
 }
 if(m->mode==5) {tsfp_x86_context_state bad=m->cpu,saved=m->cpu;
  bad.r_eip=0xdead;CHECK(!tsfp_context_transfer(&bad));
  bad=m->cpu;bad.r_esp=4095;CHECK(!tsfp_context_transfer(&bad));
  bad=m->cpu;bad.cs=0;CHECK(!tsfp_context_transfer(&bad));
  bad=m->cpu;bad.r_eflags|=0x20000;CHECK(!tsfp_context_transfer(&bad));
  CHECK(!tsfp_context_transfer(NULL));CHECK(!memcmp(&saved,&m->cpu,sizeof saved));
  CHECK(m->applies==1 && !m->rewinds && tsfp_context_execution_depth()==1);*next=0;return 2;
 }
 if(m->mode==6) {*next=0xdead;return 1;}
 if(m->mode==7)return 0;
 if(m->mode==8) {
  tsfp_x86_context_state saved=m->cpu;transfer_target(m,0x3000,800);
  CHECK(m->old_after==1 && m->applies==1 && !m->rewinds && !memcmp(&saved,&m->cpu,sizeof saved));*next=0;return 2;
 }
#ifdef _WIN32
 if(m->mode==9) {
  SwitchToFiber(m->fiber_b);
  CHECK(m->pending);longjmp(*m->pending,m->jump_code);
 }
#endif
 *next=0;return 2;
}
static void init(machine *m,unsigned mode) {
 memset(m,0,sizeof *m);m->mode=mode;m->cpu.r_eip=0x1000;m->cpu.r_esp=1000;m->cpu.cs=8;m->cpu.r_eflags=0x202;m->cpu.r_eax=0x1234;
 m->ops.opaque=m;m->ops.allow_pc=allow_pc;m->ops.allow_state=allow_state;m->ops.apply=apply;m->ops.step=step;
 m->ops.mark=mark;m->ops.rewind=rewind_scopes;m->ops.same_stack=same_stack;
}
static void contracts(void) {
 machine m;tsfp_x86_context_state saved;tsfp_context_execution_ops broken;int r;
 init(&m,0);saved=m.cpu;
 CHECK(!tsfp_context_transfer(&m.cpu));CHECK(!tsfp_context_execution_depth());
 CHECK(!tsfp_context_execute(NULL,&m.cpu,100,2000,0));
 CHECK(!tsfp_context_execute(&m.ops,NULL,100,2000,0));
 CHECK(!tsfp_context_execute(&m.ops,&m.cpu,2000,100,0));
 CHECK(!tsfp_context_execute(&m.ops,&m.cpu,100,UINT64_C(0x100000001),0));
 CHECK(!tsfp_context_execute(&m.ops,&m.cpu,100,1003,0));
 CHECK(!tsfp_context_execute(&m.ops,&m.cpu,1001,2000,0));
 m.cpu.r_eip=0;CHECK(!tsfp_context_execute(&m.ops,&m.cpu,100,2000,0));m.cpu.r_eip=0xdead;CHECK(!tsfp_context_execute(&m.ops,&m.cpu,100,2000,0));m.cpu.r_eip=0x1000;
 broken=m.ops;broken.rewind=NULL;CHECK(!tsfp_context_execute(&broken,&m.cpu,100,2000,0));
 CHECK(!memcmp(&saved,&m.cpu,sizeof saved) && !m.steps && !m.applies);
 CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0)==1);
 CHECK(m.steps==1 && m.applies==1 && !tsfp_context_execution_depth());
 init(&m,1);CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0)==1);
 CHECK(!m.old_after && m.target_seen==1 && !m.scopes && m.rewinds==1 && m.applies==2);
 CHECK(!tsfp_context_execution_depth() && !tsfp_context_transfer(&m.cpu));
 init(&m,2);CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0)==1);
 CHECK(!m.old_after && !m.outer_after && m.target_seen==1 && !m.scopes && m.rewinds==2 && m.applies==3);
 init(&m,3);CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0)==1);
 CHECK(!m.old_after && m.outer_after==1 && m.target_seen==1 && !m.scopes && m.rewinds==1 && m.applies==3);
 init(&m,10);CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0)==1);
 CHECK(!m.old_after && m.outer_after==1 && m.applies==4 && m.target_seen==1 && m.rewinds==2 && !m.scopes && !tsfp_context_execution_depth());
 init(&m,11);CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0)==1);
 CHECK(m.cpu.r_esp==1500 && m.target_seen==1 && !m.scopes && !m.old_after && !tsfp_context_execution_depth());
 init(&m,13);CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0)==1);
 CHECK(m.inner_cleaned==1 && m.rewinds==1 && m.applies==2 && m.target_seen==1 && !m.scopes && !m.old_after && !tsfp_context_execution_depth());
 init(&m,15);CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0)==TSFP_CONTEXT_EXECUTE_CLEANUP_FAILED);
 CHECK(m.inner_cleaned==1 && m.rewinds==1 && m.applies==1 && !m.target_seen && !m.scopes && !m.old_after && !tsfp_context_execution_depth());
 init(&m,16);CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0)==TSFP_CONTEXT_EXECUTE_BAD_PC);
 init(&m,17);CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0)==TSFP_CONTEXT_EXECUTE_STEP_FAILED);
 init(&m,18);CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0x2000)==1);
 CHECK(m.target_seen==1 && m.applies==2 && !m.old_after && !m.scopes && !tsfp_context_execution_depth());
 init(&m,19);CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0x1000)==1);
 CHECK(m.steps==1 && m.applies==1 && !tsfp_context_execution_depth());
 init(&m,20);CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0x2000)==1);
 CHECK(m.steps==2 && m.target_seen==1 && !tsfp_context_execution_depth());
 init(&m,21);CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0)==1);
 CHECK(m.steps==2 && m.target_seen==1 && !tsfp_context_execution_depth());
 init(&m,4);CHECK(tsfp_context_execute(&m.ops,&m.cpu,0,4096,0)==1);
 CHECK(m.applies==64 && m.target_seen==1 && !tsfp_context_execution_depth());
 init(&m,5);CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0)==1);
 init(&m,6);CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0)==TSFP_CONTEXT_EXECUTE_BAD_PC);
 init(&m,7);CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0)==TSFP_CONTEXT_EXECUTE_STEP_FAILED);
 init(&m,1);m.cleanup_fail=1;r=tsfp_context_execute(&m.ops,&m.cpu,100,2000,0);
 CHECK(r==TSFP_CONTEXT_EXECUTE_CLEANUP_FAILED && !m.old_after && !m.target_seen && m.applies==1 && !tsfp_context_execution_depth());
 init(&m,8);m.force_foreign=1;CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0)==1);
 /* A complete last word of the32-bit stack domain must not overflow. */
 init(&m,0);m.cpu.r_esp=UINT32_MAX-3;
 CHECK(tsfp_context_execute(&m.ops,&m.cpu,0,UINT64_C(0x100000000),0)==1);
 CHECK(!tsfp_context_execution_depth());
}
#ifdef _WIN32
static void foreign_jump(void *opaque,jmp_buf *env,int code,uintptr_t tag) {
 machine *m=opaque;CHECK(tag);++m->adapter_calls;
 if(m->adapter_returns)return;
 m->pending=env;m->jump_code=code;SwitchToFiber(m->fiber_a);CHECK(0);
}
static void WINAPI other_fiber(void *opaque) {
 machine *m=opaque;++m->scopes;transfer_target(m,0x3000,800);CHECK(0);
}
static void fibers(void) {
 machine m;init(&m,9);m.ops.foreign_jump=foreign_jump;
 m.fiber_a=ConvertThreadToFiber(NULL);CHECK(m.fiber_a);m.fiber_b=CreateFiber(0,other_fiber,&m);CHECK(m.fiber_b);
 CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0)==1);
 CHECK(m.adapter_calls==1 && m.target_seen==1 && m.rewinds==1 && !m.old_after && !m.scopes && !tsfp_context_execution_depth());
 DeleteFiber(m.fiber_b);CHECK(ConvertFiberToThread());
}
static DWORD WINAPI worker(void *ignored)
#else
static void *worker(void *ignored)
#endif
{
 unsigned i;(void)ignored;
 for(i=0;i<100;i++){machine m;init(&m,1);CHECK(tsfp_context_execute(&m.ops,&m.cpu,100,2000,0)==1);CHECK(m.target_seen==1 && !m.old_after && !tsfp_context_execution_depth());}
 return 0;
}
static void threads(void) {
 unsigned i;
#ifdef _WIN32
 HANDLE handles[4];for(i=0;i<4;i++){handles[i]=CreateThread(NULL,0,worker,NULL,0,NULL);CHECK(handles[i]);}
 CHECK(WaitForMultipleObjects(4,handles,TRUE,INFINITE)==WAIT_OBJECT_0);
 for(i=0;i<4;i++){DWORD code;CHECK(GetExitCodeThread(handles[i],&code) && !code);CHECK(CloseHandle(handles[i]));}
#else
 pthread_t handles[4];for(i=0;i<4;i++)CHECK(!pthread_create(&handles[i],NULL,worker,NULL));
 for(i=0;i<4;i++)CHECK(!pthread_join(handles[i],NULL));
#endif
 CHECK(!tsfp_context_execution_depth());
}
int main(int argc,char **argv) {
#ifdef _WIN32
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
#ifdef _MSC_VER
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
#endif
 if(argc>1) {
#ifdef _WIN32
  machine m;CHECK(!strcmp(argv[1],"bad-adapter"));init(&m,1);m.force_foreign=1;m.adapter_returns=1;m.ops.foreign_jump=foreign_jump;
  (void)tsfp_context_execute(&m.ops,&m.cpu,100,2000,0);CHECK(0);
#else
  (void)argv;
#endif
 }
 contracts();threads();
#ifdef _WIN32
 fibers();
#endif
 puts("PASS: live native checkpoints, validated PC/context transfer, nested root cleanup, TLS isolation and owned foreign-stack adapter");return 0;
}
