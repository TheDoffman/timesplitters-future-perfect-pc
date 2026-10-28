/* Project storage for the canonical translated-code register interface.
 * TLS is per native thread, shared by its translated callbacks and fibers.
 * Loader mappings, thread objects and saved fiber contexts remain separate.
 */
#include "recomp_types.h"

RECOMP_TLS uint32_t g_eax, g_ebx, g_ecx, g_edx;
RECOMP_TLS uint32_t g_esi, g_edi, g_esp, g_ebp, g_seh_ebp;
/* Bootstrap TIB address in the current loader's low-memory layout. Spawned
 * guest threads receive their actual TIB from that loader before execution. */
RECOMP_TLS uint32_t g_fs_base = UINT32_C(0x1000);
RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3;
RECOMP_TLS RecompXmm g_xmm4, g_xmm5, g_xmm6, g_xmm7;
RECOMP_TLS RecompMmx g_mm0, g_mm1, g_mm2, g_mm3;
RECOMP_TLS RecompMmx g_mm4, g_mm5, g_mm6, g_mm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp, g_df;
RECOMP_TLS uint16_t g_fp_control_word = UINT16_C(0x037f);
RECOMP_TLS uint16_t g_fp_cc = UINT16_C(0x4000);
RECOMP_TLS uint32_t g_icall_saved_esp, g_icall_dispatch_form;

/* These retain the existing process-wide ABI. Volatile history is diagnostic
 * best effort, not an atomic multi-producer ring or coherent crash snapshot. */
volatile uint32_t g_icall_trace[ICALL_TRACE_SIZE];
volatile uint32_t g_icall_trace_idx;
volatile uint64_t g_icall_count;
int g_force_return;
