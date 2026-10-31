/* Native macOS bring-up entry point. No Win32 executable or compatibility layer. */
#include <xbox/xboxrecomp.h>
#include "xbox_watchpoint.h"
#include "recomp_config.h"
#include "crash_posix.h"
#include "../diagnostics/pc_breadcrumbs.h"
#include "../network/pc_discord.h"
#include "../network/pc_online.h"
#include "../gameplay/pc_native_rate.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ucontext.h>

extern RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi, g_ebp;
extern ptrdiff_t g_xbox_mem_offset;
extern void xbe_entry_point(void);
extern int recomp_dispatch_init(void);
extern void xbox_path_init(const char *, const char *);
extern void xbox_SetApuInterruptSource(int (*pending)(void));

extern int tsfp_native_mmio(siginfo_t *, void *);

/* Last-resort diagnostic only: never resume a faulting guest instruction. */
static void fault(int sig, siginfo_t *info, void *context)
{
    if ((sig == SIGBUS || sig == SIGSEGV) && tsfp_native_mmio(info, context)) return;
    tsfp_native_crash_signal(sig,info,context);
}

static void capture_guest(uint32_t r[8]){r[0]=g_eax;r[1]=g_ecx;r[2]=g_edx;r[3]=g_esp;r[4]=g_ebx;r[5]=g_esi;r[6]=g_edi;r[7]=g_ebp;}

int main(int argc, char **argv)
{
    int helper_result=tsfp_native_crash_helper(argc,argv);if(helper_result>=0)return helper_result;
    if(argc==2 && strcmp(argv[1],"--online-self-test")==0) {
        extern int pc_online_self_test(void);return pc_online_self_test();
    }
    if(argc==2 && strcmp(argv[1],"--renderer-self-test")==0) {
        extern int tsfp_native_renderer_test(void);
        return tsfp_native_renderer_test();
    }

    if(argc==2 && strcmp(argv[1],"--input-self-test")==0) {
        extern int tsfp_native_input_test(void);return tsfp_native_input_test();
    }
    if(argc==2 && strcmp(argv[1],"--settings-self-test")==0) {
        extern int tsfp_native_settings_test(void);return tsfp_native_settings_test();
    }
    if(argc==2 && strcmp(argv[1],"--debug-self-test")==0) {
        extern int tsfp_native_debug_test(void);return tsfp_native_debug_test();
    }
    (void)argc; (void)argv;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    extern void xbox_SetAllocationObserver(void (*observer)(uint32_t,uint32_t,uint32_t));
    xbox_SetAllocationObserver(pc_capture_allocation);
    tsfp_native_crash_init(capture_guest);
    struct sigaction sa = {0};
    sa.sa_sigaction = fault;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);

    const char *game = getenv("RECOMP_GAME_DIR");
    const char *saves = getenv("RECOMP_SAVE_DIR");
    const char *input = getenv("RECOMP_INPUT_CONFIG");
    const char *display = getenv("RECOMP_DISPLAY_CONFIG");
    if (!game || !*game || !saves || !*saves || !input || !*input || !display || !*display) {
        fprintf(stderr, "Native bring-up requires explicit game, save, input and runtime-display configuration paths.\n");
        return 2;
    }
    int native_rate=pc_native_rate();
    if(native_rate) fprintf(stderr,"[native] target %d fresh game FPS; console clock unchanged\n",native_rate);
    else fprintf(stderr,"[native] fixed 60 FPS; interpolation disabled\n");
    char path[4096];
    if (snprintf(path, sizeof path, "%s/default.xbe", game) >= (int)sizeof path) return 2;
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 2; }
    if (fseek(f, 0, SEEK_END)) { fclose(f); return 2; }
    long size = ftell(f);
    if (size <= 0 || fseek(f, 0, SEEK_SET)) { fclose(f); return 2; }
    void *data = malloc((size_t)size);
    if (!data || fread(data, 1, (size_t)size, f) != (size_t)size) {
        fclose(f); free(data); return 2;
    }
    fclose(f);
    printf("[NATIVE-BOOT] TimeSplitters: Future Perfect — macOS ARM64\n");
    printf("[NATIVE-BOOT] XBE loaded: %ld bytes; saves: %s\n", size, saves);
    if (!xbox_MemoryLayoutInit(data, (size_t)size)) {
        fprintf(stderr, "[NATIVE-BOOT] Xbox memory initialization failed\n");
        free(data); return 1;
    }
    g_xbox_mem_offset = xbox_GetMemoryOffset();
    tsfp_native_crash_memory((uint64_t)g_xbox_mem_offset);
    printf("[NATIVE-BOOT] Xbox memory offset: %p\n", (void *)g_xbox_mem_offset);
    xbox_kernel_init();
    xbox_path_init(game, saves);
    {
        extern int xbox_save_root_utf8(char *, unsigned);
        extern void pc_storage_set_path(const char *);
        extern int xbox_set_profile_root_utf8(const char *);
        extern void pc_storage_set_profile_router(int (*)(const char *));
        pc_storage_set_profile_router(xbox_set_profile_root_utf8);
        extern int pc_storage_resolve_root(const char *,char *,size_t);
        char save_root[4096],selected[4096];
        if(xbox_save_root_utf8(save_root,sizeof save_root)) {
            int result=pc_storage_resolve_root(save_root,selected,sizeof selected);
            if(result<0){fprintf(stderr,"Selected save drive unavailable. Reconnect it or remove save-location.txt beside the controls file.\n");return 2;}
            if(result>0){xbox_path_init(game,selected);xbox_save_root_utf8(save_root,sizeof save_root);}
            pc_storage_set_path(save_root);
        }
    }
    xbox_kernel_bridge_init();
    xbox_watch_init();
    if (xbox_EnvSwitch("RECOMP_AC97_READY", 1)) {
        g_apu_state = mcpx_apu_init_standalone((uint8_t *)g_xbox_mem_offset + XBOX_CONTIG_BASE);
        if (g_apu_state) xbox_SetApuInterruptSource(mcpx_apu_irq_pending);
    }
    g_esp = XBOX_STACK_TOP;
    if (!recomp_dispatch_init()) fprintf(stderr, "[NATIVE-BOOT] Dispatch uses binary search\n");
    xbox_WatchdogStart();
    printf("[NATIVE-BOOT] Entering translated Xbox entry 0038024D; ESP=%08x\n", g_esp);
    atexit(pc_online_disconnect);
    pc_discord_start();
    tsfp_native_crash_armed(1);
    xbe_entry_point();
    tsfp_native_crash_armed(0);
    pc_discord_stop();
    printf("[NATIVE-BOOT] Guest entry returned\n");
    xbox_kernel_shutdown();
    xbox_MemoryLayoutShutdown();
    free(data);
    return 0;
}
