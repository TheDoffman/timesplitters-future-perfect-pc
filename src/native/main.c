/* Native macOS bring-up entry point. No Win32 executable or compatibility layer. */
#include <xbox/xboxrecomp.h>
#include "xbox_watchpoint.h"
#include "recomp_config.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ucontext.h>

extern RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
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
    ucontext_t *uc = context;
    uintptr_t pc = uc->uc_mcontext->__ss.__pc;
    uint32_t ins; memcpy(&ins, (void *)pc, sizeof ins);
    char line[512];
    int n = snprintf(line, sizeof line,
        "\n[NATIVE-FAULT] signal=%d address=%p guest-offset=%p "
        "pc=%p instruction=%08x eax=%08x ecx=%08x edx=%08x esp=%08x ebx=%08x esi=%08x edi=%08x\n",
        sig, info->si_addr, (void *)g_xbox_mem_offset,
        (void *)pc, ins, g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi);
    if (n > 0) write(STDERR_FILENO, line, (size_t)n < sizeof line ? (size_t)n : sizeof line - 1);
    _Exit(128 + sig);
}

int main(int argc, char **argv)
{
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
    (void)argc; (void)argv;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    struct sigaction sa = {0};
    sa.sa_sigaction = fault;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);

    const char *game = getenv("RECOMP_GAME_DIR");
    const char *saves = getenv("RECOMP_SAVE_DIR");
    const char *input = getenv("RECOMP_INPUT_CONFIG");
    const char *display = getenv("RECOMP_DISPLAY_CONFIG");
    if (!game || !*game || !saves || !*saves || !input || !*input || !display || !*display) {
        fprintf(stderr, "Native bring-up requires explicit game, save, input and runtime-display configuration paths.\n");
        return 2;
    }
    /* Native display frames are capped at 2x; console timing remains 60 Hz.
     * A saved frame_interp=1 still selects the original 60 FPS path. */
    int multiple=recomp_config_int("RECOMP_FRAME_INTERP","frame_interp",2)>=2?2:1;
    setenv("RECOMP_FRAME_INTERP",multiple==2?"2":"1",1);
    if(multiple==2 && !recomp_config_lookup("RECOMP_FPS_CAP","frame_cap"))
        setenv("RECOMP_FPS_CAP","60",0);
    fprintf(stderr,"[native-gl] target %d display FPS; console clock unchanged\n",multiple*60);
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
    printf("[NATIVE-BOOT] Xbox memory offset: %p\n", (void *)g_xbox_mem_offset);
    xbox_kernel_init();
    xbox_path_init(game, saves);
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
    xbe_entry_point();
    printf("[NATIVE-BOOT] Guest entry returned\n");
    xbox_kernel_shutdown();
    xbox_MemoryLayoutShutdown();
    free(data);
    return 0;
}
