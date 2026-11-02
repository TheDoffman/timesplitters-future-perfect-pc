/* Original bounded Win32 oracle; temporary FS records belong to this process. */
#pragma warning(disable:4733)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
typedef void (NTAPI *raise_t)(EXCEPTION_RECORD *);
typedef EXCEPTION_DISPOSITION (__cdecl *handler_t)(EXCEPTION_RECORD *,void *,CONTEXT *,void *);
typedef struct node {struct node *next;handler_t handler;} node;
static raise_t raise_fn;
static node frames[2];
static void *original_head;
static EXCEPTION_RECORD record;
static unsigned mode,count,route,before_sp,after_sp,wrapper_bp,after_bp,after_eax;
static unsigned ordinary_label,edited_label,seen[2][14];
static EXCEPTION_DISPOSITION __cdecl handler(EXCEPTION_RECORD *r,void *frame,CONTEXT *c,void *dispatcher){
 unsigned head,i=count++;__asm {mov eax,fs:[0]
                              mov head,eax}
 if(i>=2)ExitProcess(3);
 seen[i][0]=r->ExceptionCode;seen[i][1]=r->ExceptionFlags;seen[i][2]=c->ContextFlags;
 seen[i][3]=c->Eip==ordinary_label;seen[i][4]=c->Esp==before_sp;seen[i][5]=c->Ebp==wrapper_bp;
 seen[i][6]=c->Eax;seen[i][7]=c->Ebx;seen[i][8]=c->Ecx;seen[i][9]=c->Edx;
 seen[i][10]=(unsigned)dispatcher;seen[i][11]=((node*)head)->next==(node*)frame;
 seen[i][12]=(unsigned)r->ExceptionAddress==ordinary_label;seen[i][13]=c->Eip==edited_label;
 if((mode&2) && !i){c->Eip=edited_label;c->Eax=0x87654321;}
 if((mode&4) && !i){*(void **)dispatcher=(void *)((node *)frame)->next;return ExceptionNestedException;}
 return (mode&1) && !i?ExceptionContinueSearch:ExceptionContinueExecution;
}
__declspec(naked) static void exercise(void){__asm {
 push ebp
 mov ebp,esp
 pushad
 mov before_sp,esp
 mov wrapper_bp,ebp
 lea eax,ordinary
 mov ordinary_label,eax
 lea eax,edited
 mov edited_label,eax
 mov ebx,011223344h
 mov ecx,055667788h
 mov edx,099aabbcch
 mov esi,013579bdfh
 mov edi,02468ace0h
 push offset record
 mov eax,0deadbeefh
 stc
 call raise_fn
 ordinary:
 mov route,1
 jmp finished
 edited:
 mov route,2
 finished:
 mov after_eax,eax
 mov after_sp,esp
 mov after_bp,ebp
 mov esp,before_sp
 popad
 pop ebp
 ret
}}
int main(void){
 unsigned attempt,i;void *head_after;
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 raise_fn=(raise_t)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlRaiseException");if(!raise_fn)return 2;
 __asm {mov eax,fs:[0]
        mov original_head,eax}
 for(attempt=0;attempt<8;attempt++){
  node local[2];mode=attempt;count=route=0;memset(seen,0,sizeof seen);memset(&record,0,sizeof record);
  record.ExceptionCode=0xe0424771;record.ExceptionAddress=(void*)0x1234;record.NumberParameters=2;
  record.ExceptionInformation[0]=0x11223344;record.ExceptionInformation[1]=0x55667788;
  local[0].next=&local[1];local[1].next=original_head;local[0].handler=local[1].handler=handler;
  __asm {lea eax,local
         mov fs:[0],eax}
  exercise();
  __asm {mov eax,fs:[0]
         mov head_after,eax
         mov eax,original_head
         mov fs:[0],eax}
  printf("case%u route%u eax%08x sp%u bp%u head%u handlers%u flags%08lx address%u params%u\n",attempt,route,after_eax,before_sp==after_sp,wrapper_bp==after_bp,head_after==&local[0],count,record.ExceptionFlags,(unsigned)record.ExceptionAddress==ordinary_label,record.NumberParameters==2 && record.ExceptionInformation[0]==0x11223344 && record.ExceptionInformation[1]==0x55667788);
  for(i=0;i<count;i++)printf("handler%u code%08x flags%08x ctxflags%08x ordinary%u sp%u bp%u eax%08x ebx%08x ecx%08x edx%08x dispatcher%08x wrapped%u address%u edited%u\n",i,seen[i][0],seen[i][1],seen[i][2],seen[i][3],seen[i][4],seen[i][5],seen[i][6],seen[i][7],seen[i][8],seen[i][9],seen[i][10],seen[i][11],seen[i][12],seen[i][13]);
 }
 (void)frames;return 0;
}
