/* Original Win32 x86 oracle; owns and restores temporary stack FS records.
 * Context edits drive a real native continuation; no guest code is used. */
#pragma warning(disable:4733)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
typedef VOID (NTAPI *unwind_t)(PVOID,PVOID,PEXCEPTION_RECORD,PVOID);
typedef EXCEPTION_DISPOSITION (__cdecl *handler_t)(EXCEPTION_RECORD *,void *,CONTEXT *,void *);
typedef struct node {struct node *next;handler_t handler;} node;
static unwind_t unwind_fn;
static void *target_frame,*original_head,*target_ip;
static EXCEPTION_RECORD record,*record_arg;
static unsigned route,observed_eax,before_sp,after_sp,wrapper_bp,after_bp,seen_count;
static unsigned redirect,resume_label;
static unsigned seen[4][10];
static EXCEPTION_DISPOSITION __cdecl handler(EXCEPTION_RECORD *r,void *frame,CONTEXT *c,void *dispatcher) {
 unsigned head;
 __asm { mov eax,fs:[0]
         mov head,eax }
 unsigned i=seen_count++;
 if(i<4) {seen[i][0]=r->ExceptionCode;seen[i][1]=r->ExceptionFlags;seen[i][2]=(unsigned)frame;seen[i][3]=c?c->ContextFlags:0;seen[i][4]=c?c->Eip:0;seen[i][5]=c?c->Esp:0;seen[i][6]=c?c->Eax:0;seen[i][7]=(unsigned)dispatcher;seen[i][8]=head==(unsigned)frame;seen[i][9]=((node*)head)->next==(node*)frame;}
 if(redirect && c){c->Eip=resume_label;c->Eax=0x87654321;}
 return ExceptionContinueSearch;
}
__declspec(naked) static void exercise(void) {
 __asm {
  push ebp
  mov ebp,esp
  pushad
  mov before_sp,esp
  mov wrapper_bp,ebp
  lea eax,target_label
  mov resume_label,eax
  cmp target_ip,0
  je argument_ready
  lea eax,target_label
  mov target_ip,eax
 argument_ready:
  push 012345678h
  push record_arg
  push target_ip
  push target_frame
  call unwind_fn
  mov route,1
  jmp finished
 target_label:
  mov route,2
 finished:
  mov observed_eax,eax
  mov after_sp,esp
  mov after_bp,ebp
  mov esp,before_sp
  popad
  pop ebp
  ret
 }
}
int main(void) {
 node frames[2];unsigned attempt;void *head_after;
 unwind_fn=(unwind_t)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlUnwind");if(!unwind_fn)return 2;
 __asm {mov eax,fs:[0]
        mov original_head,eax}
 for(attempt=0;attempt<16;attempt++) {
  memset(&record,0,sizeof record);record.ExceptionCode=0xe1234567;record.ExceptionFlags=1;record.NumberParameters=2;record.ExceptionInformation[0]=0x11223344;record.ExceptionInformation[1]=0x55667788;
  frames[0].next=&frames[1];frames[1].next=(node*)original_head;frames[0].handler=frames[1].handler=handler;
  target_frame=(attempt&1)?original_head:&frames[1];target_ip=(void*)((attempt&2)?1:0);record_arg=(attempt&4)?&record:NULL;
  redirect=!!(attempt&8);seen_count=route=0;memset(seen,0,sizeof seen);
  __asm {lea eax,frames
         mov fs:[0],eax}
  exercise();
  __asm {mov eax,fs:[0]
         mov head_after,eax
         mov eax,original_head
         mov fs:[0],eax}
  printf("case%u route%u eax%08x sp_equal%u bp_equal%u head_target%u handlers%u inputflags%08lx\n",attempt,route,observed_eax,before_sp==after_sp,wrapper_bp==after_bp,head_after==target_frame,seen_count,record.ExceptionFlags);
  {unsigned i;for(i=0;i<seen_count && i<4;i++)printf("handler%u code%08x flags%08x frame_index%u contextflags%08x context_eax%08x dispatcher%08x head_is_frame%u head_next_frame%u\n",i,seen[i][0],seen[i][1],seen[i][2]==(unsigned)&frames[0]?0:1,seen[i][3],seen[i][6],seen[i][7],seen[i][8],seen[i][9]);}
 }
 return 0;
}
