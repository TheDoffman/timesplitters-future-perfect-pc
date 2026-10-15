#include <sys/mman.h>
#include <unistd.h>
#define g_esp prompt_test_esp
#define g_xbox_mem_offset prompt_test_offset
static uint32_t prompt_test_esp;
static ptrdiff_t prompt_test_offset;
#include "../pc_prompt_guest.inc"
#undef g_esp
#undef g_xbox_mem_offset
static char prompt_test_text[2048];
static void prompt_test_original(void) {
    const uint32_t *frame=(const uint32_t*)(prompt_test_offset+prompt_test_esp);
    snprintf(prompt_test_text,sizeof prompt_test_text,"%s",(const char*)(prompt_test_offset+frame[3]));
    prompt_test_esp+=4;
}
static int native_test_prompts(void) {
    int ok=1;pc_prompts_reset();char text[128];
    ok &= pc_prompts_rewrite("\v2 Select  \v1 Back",text,sizeof text);
    ok &= strstr(text,"[E] Select") && strstr(text,"[F] Back");
    int keys[PC_ACTION_COUNT]={0};keys[0]='Z';
    ok &= pc_prompts_format("\v2 Select  \v1 Back  \vE Look",text,sizeof text,keys,1);
    ok &= !strcmp(text,"[Z] Select  [Unbound] Back  [Mouse] Look");
    char tiny[2];ok &= !pc_prompts_format("\v2",tiny,sizeof tiny,keys,1);
    RecompPadState pad={0};pad.buttons=1;pc_prompts_controller(&pad,1);
    ok &= !pc_prompts_keyboard_active();
    SDL_Event e={0};e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=SDL_SCANCODE_E;
    tsfp_native_input_event(&e,NULL);ok &= pc_prompts_keyboard_active();
    pc_prompts_controller(&pad,1);ok &= pc_prompts_keyboard_active();
    pad.buttons=0;pad.lx=500;pc_prompts_controller(&pad,1);ok &= pc_prompts_keyboard_active();
    pad.rx=20000;pc_prompts_controller(&pad,1);ok &= !pc_prompts_keyboard_active();
    e.type=SDL_EVENT_KEY_UP;tsfp_native_input_event(&e,NULL);ok &= !pc_prompts_keyboard_active();
    pc_prompts_reset();
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    BYTE *memory=mmap(NULL,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
    if(memory==MAP_FAILED)return 0;
    ok &= mprotect(memory+page,page,PROT_NONE)==0;
    ok &= pc_guest_memory((uintptr_t)memory,32,1);
    ok &= !pc_guest_memory((uintptr_t)(memory+page-8),16,0);
    ok &= !pc_guest_memory((uintptr_t)(memory+page),1,0);
    ok &= !pc_guest_memory(UINTPTR_MAX-4,16,0);
    prompt_test_offset=(ptrdiff_t)memory;prompt_test_esp=(uint32_t)(page/2);
    uint32_t *stack=(uint32_t*)(memory+prompt_test_esp);
    stack[0]=0x12345678;stack[1]=0x44;stack[2]=0x55;stack[3]=128;stack[4]=1;
    strcpy((char*)memory+128,"\v2 Select");
    pc_prompt_guest(prompt_test_original,3,4);
    ok &= prompt_test_esp==page/2+4 && stack[0]==0x12345678 && stack[3]==128;
    ok &= !strcmp(prompt_test_text,"[E] Select");
    ok &= mprotect(memory,page,PROT_READ)==0;
    ok &= pc_guest_memory((uintptr_t)memory,32,0) && !pc_guest_memory((uintptr_t)memory,32,1);
    munmap(memory,page*2);
    fprintf(stderr,"[native-prompt-test] bindings, device activity, guest ABI and VM protection: %s\n",ok?"PASS":"FAIL");
    return ok;
}
