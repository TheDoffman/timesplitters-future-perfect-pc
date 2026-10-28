/* TimeSplitters guest execution interface.
 * Project-owned primitives shared by emitted C and host adapters. Guest values
 * use 32-bit addresses and thread-local architectural cells; C call frames carry
 * native control flow. Services declared here are implemented separately and
 * several still use the transitional runtime. This header does not own them.
 */
#ifndef RECOMP_TYPES_H
#define RECOMP_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif
#if !defined(_MSC_VER)
#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif
#ifndef __debugbreak
#define __debugbreak() __builtin_trap()
#endif
#endif

/* A divide fault stops before writing result registers. */
#if defined(_MSC_VER)
__declspec(noreturn)
#else
__attribute__((noreturn))
#endif
void tsfp_guest_divide_error(uint32_t site, unsigned reason);

/* Relocation is applied after guest-width arithmetic. Executable bounds are
 * supplied by the loader; a zero upper bound admits calls during initialization.
 * Synthetic kernel entries occupy the high address window. */
extern ptrdiff_t g_xbox_mem_offset;
extern uint32_t g_xbox_code_lo, g_xbox_code_hi;
extern int g_force_return;
static inline int tsfp_guest_target_is_code(uint32_t address)
{
    if (address >= UINT32_C(0xfe000000)) return 1;
    if (g_xbox_code_hi == 0) return 1;
    if (address < g_xbox_code_lo) return 0;
    return address < g_xbox_code_hi;
}
#define RECOMP_ICALL_IS_CODE(address) tsfp_guest_target_is_code((uint32_t)(address))


/* Optional title numeric policy. The generic runtime retains x86 conversion
 * semantics; a title can supply a separate header without baking its addresses
 * into the instruction lifter. Memory operands are evaluated only once. */
#ifdef RECOMP_NUMERIC_POLICY_HEADER
#include RECOMP_NUMERIC_POLICY_HEADER
#else
#define RECOMP_I32_TO_F32(address, value) ((float)(int32_t)(value))
#define RECOMP_I32_TO_F64(address, value) ((double)(int32_t)(value))
#endif
#ifndef RECOMP_I32_TO_F32_AT
#define RECOMP_I32_TO_F32_AT(site, address, value) RECOMP_I32_TO_F32(address, value)
#endif
#ifndef RECOMP_STORE_GLOBAL_U32
#define RECOMP_STORE_GLOBAL_U32(site, address, value) (value)
#endif
#ifndef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site, address, value) (value)
#endif
#ifndef RECOMP_LOAD_U8_AT
#define RECOMP_LOAD_U8_AT(site, value) (value)
#endif
#ifndef RECOMP_PUSH_IMM_U32_AT
#define RECOMP_PUSH_IMM_U32_AT(site, value) (value)
#endif
/* A title may complete an audited leaf after its entry callback. Returning
 * zero must leave guest state untouched so the generated body can run. */
#ifndef RECOMP_FAST_ENTRY
#define RECOMP_FAST_ENTRY(site) 0
#endif
#ifndef RECOMP_DIRECT_CALLEE_AT
#define RECOMP_DIRECT_CALLEE_AT(site, function) (function)
#endif
#ifndef RECOMP_FLOAT_BRANCH_AT
#define RECOMP_FLOAT_BRANCH_AT(site, taken) (taken)
#endif
#ifndef RECOMP_F32_ADD_AT
#define RECOMP_F32_ADD_AT(site, left, right) ((left) + (right))
#endif
#ifndef RECOMP_F32_SUB_AT
#define RECOMP_F32_SUB_AT(site, left, right) ((left) - (right))
#endif
#ifndef RECOMP_F32_MUL_AT
#define RECOMP_F32_MUL_AT(site, left, right) ((left) * (right))
#endif
#ifndef RECOMP_F32_DIV_AT
#define RECOMP_F32_DIV_AT(site, left, right) ((left) / (right))
#endif

#ifndef RECOMP_AFTER_STORE32
#define RECOMP_AFTER_STORE32(site, address) ((void)0)
#endif

/* TSFP guest register interface begin.
 * Architectural values belong to the current host thread. Generated routines
 * share these cells with callbacks; EBP remains function-local, with separate
 * frame/SEH bridges. The double-backed x87 stack and independent MMX storage
 * retain the current execution model, not the physical x87/MMX alias model.
 */
#ifndef RECOMP_TLS
#if defined(_MSC_VER)
#define RECOMP_TLS __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define RECOMP_TLS __thread
#else
#define RECOMP_TLS _Thread_local
#endif
#endif

#ifndef RECOMP_XMM_DEFINED
#define RECOMP_XMM_DEFINED
typedef union RecompXmm {
    uint64_t q[2];
    uint32_t u[4];
    int32_t i[4];
    float f[4];
    double d[2];
} RecompXmm;
#endif
#ifndef RECOMP_MMX_DEFINED
#define RECOMP_MMX_DEFINED
typedef union RecompMmx {
    uint64_t q;
    uint32_t ud[2];
    int32_t d[2];
    uint16_t uw[4];
    int16_t w[4];
    uint8_t ub[8];
    int8_t b[8];
} RecompMmx;
#endif

extern RECOMP_TLS uint32_t g_eax, g_ebx, g_ecx, g_edx;
extern RECOMP_TLS uint32_t g_esi, g_edi, g_esp;
extern RECOMP_TLS uint32_t g_ebp, g_seh_ebp, g_fs_base;
extern RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3;
extern RECOMP_TLS RecompXmm g_xmm4, g_xmm5, g_xmm6, g_xmm7;
extern RECOMP_TLS RecompMmx g_mm0, g_mm1, g_mm2, g_mm3;
extern RECOMP_TLS RecompMmx g_mm4, g_mm5, g_mm6, g_mm7;
extern RECOMP_TLS double g_fp_stack[8];
extern RECOMP_TLS int g_fp_top;
extern RECOMP_TLS int g_fp_cmp, g_df;
extern RECOMP_TLS uint16_t g_fp_control_word;
extern RECOMP_TLS uint16_t g_fp_cc;
#define XBOX_FS_BASE g_fs_base
/* TSFP guest register interface end. */

/* TSFP own atomics begin: sequentially consistent operations on mapped RAM.
 * Naturally aligned operands follow the host intrinsic contract. Guest MMIO,
 * alignment/page-crossing faults and guest exception delivery are separate.
 * Unlocked XADD/CMPXCHG retain the existing stronger atomic-memory policy. */
#ifndef TSFP_GUEST_ATOMIC_PRIMITIVES
#define TSFP_GUEST_ATOMIC_PRIMITIVES
#include <stdint.h>
#if defined(_MSC_VER)
#include <intrin.h>
static inline uint8_t tsfp_atomic_exchange8(void *address, uint32_t value) {
    return (uint8_t)_InterlockedExchange8((volatile char *)address, (char)value);
}
static inline uint8_t tsfp_atomic_add8(void *address, uint32_t value) {
    return (uint8_t)_InterlockedExchangeAdd8((volatile char *)address, (char)value);
}
static inline uint8_t tsfp_atomic_cas8(void *address, uint32_t compare, uint32_t value) {
    return (uint8_t)_InterlockedCompareExchange8((volatile char *)address, (char)value, (char)compare);
}
static inline uint16_t tsfp_atomic_exchange16(void *address, uint32_t value) {
    return (uint16_t)_InterlockedExchange16((volatile short *)address, (short)value);
}
static inline uint16_t tsfp_atomic_add16(void *address, uint32_t value) {
    return (uint16_t)_InterlockedExchangeAdd16((volatile short *)address, (short)value);
}
static inline uint16_t tsfp_atomic_cas16(void *address, uint32_t compare, uint32_t value) {
    return (uint16_t)_InterlockedCompareExchange16((volatile short *)address, (short)value, (short)compare);
}
static inline uint32_t tsfp_atomic_exchange32(void *address, uint32_t value) {
    return (uint32_t)_InterlockedExchange((volatile long *)address, (long)value);
}
static inline uint32_t tsfp_atomic_add32(void *address, uint32_t value) {
    return (uint32_t)_InterlockedExchangeAdd((volatile long *)address, (long)value);
}
static inline uint32_t tsfp_atomic_cas32(void *address, uint32_t compare, uint32_t value) {
    return (uint32_t)_InterlockedCompareExchange((volatile long *)address, (long)value, (long)compare);
}
#else
static inline uint8_t tsfp_atomic_exchange8(void *address, uint32_t value) {
    return __atomic_exchange_n((uint8_t *)address, (uint8_t)value, __ATOMIC_SEQ_CST);
}
static inline uint8_t tsfp_atomic_add8(void *address, uint32_t value) {
    return __atomic_fetch_add((uint8_t *)address, (uint8_t)value, __ATOMIC_SEQ_CST);
}
static inline uint8_t tsfp_atomic_cas8(void *address, uint32_t compare, uint32_t value) {
    uint8_t expected = (uint8_t)compare;
    __atomic_compare_exchange_n((uint8_t *)address, &expected, (uint8_t)value, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return expected;
}
static inline uint16_t tsfp_atomic_exchange16(void *address, uint32_t value) {
    return __atomic_exchange_n((uint16_t *)address, (uint16_t)value, __ATOMIC_SEQ_CST);
}
static inline uint16_t tsfp_atomic_add16(void *address, uint32_t value) {
    return __atomic_fetch_add((uint16_t *)address, (uint16_t)value, __ATOMIC_SEQ_CST);
}
static inline uint16_t tsfp_atomic_cas16(void *address, uint32_t compare, uint32_t value) {
    uint16_t expected = (uint16_t)compare;
    __atomic_compare_exchange_n((uint16_t *)address, &expected, (uint16_t)value, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return expected;
}
static inline uint32_t tsfp_atomic_exchange32(void *address, uint32_t value) {
    return __atomic_exchange_n((uint32_t *)address, (uint32_t)value, __ATOMIC_SEQ_CST);
}
static inline uint32_t tsfp_atomic_add32(void *address, uint32_t value) {
    return __atomic_fetch_add((uint32_t *)address, (uint32_t)value, __ATOMIC_SEQ_CST);
}
static inline uint32_t tsfp_atomic_cas32(void *address, uint32_t compare, uint32_t value) {
    uint32_t expected = (uint32_t)compare;
    __atomic_compare_exchange_n((uint32_t *)address, &expected, (uint32_t)value, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return expected;
}
#endif
/* Existing consumers can keep the old interface while the runtime is replaced. */
#define RECOMP_ATOMIC_ADD32(p, v) tsfp_atomic_add32((void *)(p), (v))
#define RECOMP_ATOMIC_CAS32(p, c, v) tsfp_atomic_cas32((void *)(p), (c), (v))
#endif /* TSFP_GUEST_ATOMIC_PRIMITIVES */
/* TSFP own atomics end */

/* Host service boundary. These names are the existing generated-code ABI.
 * Declaring a service here does not replace its implementation or claim guest
 * exception unwinding, loader ownership, or a new clock implementation. */
void recomp_debug_service(uint32_t service, uint32_t argument);
void recomp_int3_reached(uint32_t address);
void recomp_cxx_throw(uint32_t object, uint32_t information);
void recomp_unimpl(const char *instruction, uint32_t address);
void recomp_stub_missing(uint32_t address);
uint64_t xbox_ReadTimeStampCounter(void);

/* Native setjmp must execute in the caller's live frame. A foreign-stack jump
 * is handed to the fiber adapter; the guest address identifies the saved slot. */
jmp_buf *recomp_setjmp_slot(uint32_t guest_buffer);
int recomp_guest_longjmp(uint32_t guest_buffer, uint32_t result);
typedef void (*recomp_foreign_longjmp_fn)(jmp_buf *, int, uintptr_t);
void recomp_set_foreign_longjmp(recomp_foreign_longjmp_fn handler);

/* The dispatch source can declare this public type after including us. The
 * private entry type in the call primitives remains usable in either order. */
#ifndef RECOMP_DISPATCH_H
typedef void (*recomp_func_t)(void);
recomp_func_t recomp_lookup(uint32_t address);
recomp_func_t recomp_lookup_manual(uint32_t address);
recomp_func_t recomp_lookup_kernel(uint32_t address);
int recomp_dispatch_init(void);
size_t recomp_dispatch_flat_bytes(void);
#endif

/* Crash history is a 16-entry ring. Refusal context belongs to the thread;
 * the counters/ring retain the existing volatile, non-atomic service ABI. */
#define ICALL_TRACE_SIZE 16
extern volatile uint32_t g_icall_trace[ICALL_TRACE_SIZE];
extern volatile uint32_t g_icall_trace_idx;
extern volatile uint64_t g_icall_count;
extern RECOMP_TLS uint32_t g_icall_saved_esp, g_icall_dispatch_form;
void recomp_icall_fail_log(uint32_t address);
void recomp_icall_not_code_log(uint32_t address, uint32_t stack);

/* Disabled feedback must not expand or evaluate the supplied tokens. */
#ifdef RECOMP_ICALL_FEEDBACK
#include "tsfp_branch_feedback.h"
extern volatile uint64_t g_icall_guard_hits, g_icall_guard_misses;
static inline void tsfp_guest_guard_hit(void) { tsfp_feedback_guard(1); }
static inline void tsfp_guest_guard_miss(void) { tsfp_feedback_guard(0); }
#define RECOMP_ICALL_GUARD_HIT() tsfp_guest_guard_hit()
#define RECOMP_ICALL_GUARD_MISS() tsfp_guest_guard_miss()
#else
#define RECOMP_ICALL_OBSERVE(address, flags) ((void)0)
#define RECOMP_ICALL_OBSERVE_SITE(site, address) ((void)0)
#define RECOMP_ICALL_GUARD_HIT() ((void)0)
#define RECOMP_ICALL_GUARD_MISS() ((void)0)
#endif

void recomp_trace_enter(const char *name, uint32_t address);
void recomp_trace_exit(const char *name, uint32_t address);
void recomp_trace_esp(const char *name, const char *tag);
#define RECOMP_TRACE_ENTER recomp_trace_enter
#define RECOMP_TRACE_EXIT recomp_trace_exit
#define RECOMP_TRACE_ESP recomp_trace_esp
#define RECOMP_UNIMPL recomp_unimpl


/* TSFP string direction: sample DF before evaluating the byte count once.
 * Generated string instructions consume the displacement modulo 2^32. */
#define RECOMP_DF_STEP(bytes) ((g_df != 0) ? (0u - (uint32_t)(bytes)) : (uint32_t)(bytes))

/* TSFP guest primitive support begin.
 * The supported hosts are little-endian. Guest RAM accesses may be unaligned
 * and may alias any scalar representation. Volatile preserves the requested
 * access order; it is not a substitute for atomic operations or MMIO routing.
 */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Guest scalar lvalues require a little-endian host"
#endif
#if defined(__GNUC__) || defined(__clang__)
#define TSFP_GUEST_SCALAR(name, type) typedef type name __attribute__((aligned(1), may_alias))
#define TSFP_GUEST_UNALIGNED
#elif defined(_MSC_VER)
#define TSFP_GUEST_SCALAR(name, type) typedef type name
#define TSFP_GUEST_UNALIGNED __unaligned
#else
#error "Define guest unaligned scalar access for this compiler"
#endif
TSFP_GUEST_SCALAR(TsfpGuestU8, uint8_t);
TSFP_GUEST_SCALAR(TsfpGuestU16, uint16_t);
TSFP_GUEST_SCALAR(TsfpGuestU32, uint32_t);
TSFP_GUEST_SCALAR(TsfpGuestU64, uint64_t);
TSFP_GUEST_SCALAR(TsfpGuestI8, int8_t);
TSFP_GUEST_SCALAR(TsfpGuestI16, int16_t);
TSFP_GUEST_SCALAR(TsfpGuestI32, int32_t);
TSFP_GUEST_SCALAR(TsfpGuestI64, int64_t);
TSFP_GUEST_SCALAR(TsfpGuestF32, float);
TSFP_GUEST_SCALAR(TsfpGuestF64, double);
#undef TSFP_GUEST_SCALAR
#define TSFP_GUEST_CELL(type, address) (*(volatile type TSFP_GUEST_UNALIGNED *)XBOX_PTR(address))

/* Return the complete updated register, as an assignment expression does.
 * Both the cell expression and incoming value are sampled once. */
static inline uint32_t tsfp_guest_insert_bits(volatile uint32_t *cell,
                                             uint32_t value, unsigned bits,
                                             unsigned shift)
{
    const uint32_t low = (1u << bits) - 1u;
    const uint32_t prior = *cell;
    const uint32_t result = (prior & ~(low << shift)) | ((value & low) << shift);
    *cell = result;
    return result;
}
/* TSFP guest primitive support end. */

/* Wrap the guest address before adding the host relocation. Cast the offset
 * to uintptr_t to retain modular pointer arithmetic for negative relocation. */
#define XBOX_PTR(address) ((uintptr_t)(uint32_t)(address) + (uintptr_t)g_xbox_mem_offset)
#define MEM8(address) TSFP_GUEST_CELL(TsfpGuestU8, address)
#define MEM16(address) TSFP_GUEST_CELL(TsfpGuestU16, address)
#define MEM32(address) TSFP_GUEST_CELL(TsfpGuestU32, address)
#define MEM64(address) TSFP_GUEST_CELL(TsfpGuestU64, address)
#define SMEM8(address) TSFP_GUEST_CELL(TsfpGuestI8, address)
#define SMEM16(address) TSFP_GUEST_CELL(TsfpGuestI16, address)
#define SMEM32(address) TSFP_GUEST_CELL(TsfpGuestI32, address)
#define SMEM64(addr) TSFP_GUEST_CELL(TsfpGuestI64, addr)
#define MEMF(address) TSFP_GUEST_CELL(TsfpGuestF32, address)
#define MEMD(address) TSFP_GUEST_CELL(TsfpGuestF64, address)

/* TSFP partial-register and stack primitives begin. */
#define LO8(value) ((uint8_t)(value))
#define HI8(value) ((uint8_t)((uint32_t)(value) >> 8u))
#define LO16(value) ((uint16_t)(value))
#define SET_LO8(cell, value) tsfp_guest_insert_bits(&(cell), (uint32_t)(value), 8u, 0u)
#define SET_HI8(cell, value) tsfp_guest_insert_bits(&(cell), (uint32_t)(value), 8u, 8u)
#define SET_LO16(cell, value) tsfp_guest_insert_bits(&(cell), (uint32_t)(value), 16u, 0u)

/* Read a PUSH source against the old stack. POP increments before evaluating
 * its destination, including ESP itself and memory addressed through ESP. */
#define PUSH32(stack, source) do { \
    const uint32_t tsfp_pushed_ = (uint32_t)(source); \
    uint32_t *const tsfp_stack_cell_ = &(stack); \
    const uint32_t tsfp_next_stack_ = *tsfp_stack_cell_ - 4u; \
    *tsfp_stack_cell_ = tsfp_next_stack_; \
    MEM32(tsfp_next_stack_) = tsfp_pushed_; \
} while (0)
#define POP32(stack, destination) do { \
    uint32_t *const tsfp_stack_cell_ = &(stack); \
    const uint32_t tsfp_old_stack_ = *tsfp_stack_cell_; \
    const uint32_t tsfp_popped_ = MEM32(tsfp_old_stack_); \
    *tsfp_stack_cell_ = tsfp_old_stack_ + 4u; \
    (destination) = tsfp_popped_; \
} while (0)
/* TSFP partial-register and stack primitives end. */

/* TSFP scalar vector construction begin.
 * Model transforms emulate a memory MOVSS: preserve the float representation
 * in lane zero and clear the other three lanes. SIMD instructions themselves
 * are emitted directly by the project SSE/MMX translators. */
static inline RecompXmm XMM_SCALAR(float value)
{
    RecompXmm lanes;
    memcpy(&lanes.u[0], &value, sizeof(value));
    lanes.u[1] = 0u;
    lanes.u[2] = 0u;
    lanes.u[3] = 0u;
    return lanes;
}
/* TSFP scalar vector construction end. */

/* TSFP call checks begin.
 * Diagnostic builds inspect the preserved registers and guest return stack.
 * The address and cleanup expressions stay lazy, including the historical
 * short-circuit behavior of a known-pop call. Production builds call directly.
 */
#ifdef RECOMP_ABI_CHECK
typedef struct {
    uint32_t base, source, destination, stack;
} tsfp_call_snapshot;

static inline tsfp_call_snapshot tsfp_capture_call(void) {
    tsfp_call_snapshot state;
    state.base = g_ebx;
    state.source = g_esi;
    state.destination = g_edi;
    state.stack = g_esp;
    return state;
}

static inline int tsfp_call_registers_match(const tsfp_call_snapshot *state) {
    return g_ebx == state->base && g_esi == state->source &&
           g_edi == state->destination;
}

void recomp_abi_violation_log(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
void recomp_abi_pop_violation_log(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
#define RECOMP_ABI_CALL(address, target) do { \
    const tsfp_call_snapshot tsfp_before_call_ = tsfp_capture_call(); \
    (target)(); \
    if (!tsfp_call_registers_match(&tsfp_before_call_) || \
        g_esp < tsfp_before_call_.stack + 4u) \
        recomp_abi_violation_log((address), tsfp_before_call_.base, \
            tsfp_before_call_.source, tsfp_before_call_.destination, tsfp_before_call_.stack); \
} while (0)
#define RECOMP_ABI_CALL_POP(address, target, cleanup) do { \
    const tsfp_call_snapshot tsfp_before_call_ = tsfp_capture_call(); \
    (target)(); \
    if (!tsfp_call_registers_match(&tsfp_before_call_) || \
        g_esp != tsfp_before_call_.stack + 4u + (cleanup)) \
        recomp_abi_pop_violation_log((address), tsfp_before_call_.base, \
            tsfp_before_call_.source, tsfp_before_call_.destination, \
            tsfp_before_call_.stack, (cleanup)); \
} while (0)
#else
#define RECOMP_ABI_CALL(address, target) (target)()
#define RECOMP_ABI_CALL_POP(address, target, cleanup) (target)()
#endif
/* TSFP call checks end. */

/* TSFP indirect dispatch support begin.
 * Call sites supply a guest return address before entering this interface.
 * Keep lookup/notification order in one place; recovery stays at the call
 * site so saved-stack and feedback expressions retain their lazy evaluation.
 */
/* The generated table defines RECOMP_DISPATCH_H before including its types
 * header, then declares the public pointer type later. These compatible private
 * declarations keep inline helpers valid in either include order. */
typedef void (*tsfp_guest_entry_t)(void);
tsfp_guest_entry_t recomp_lookup_manual(uint32_t target);
tsfp_guest_entry_t recomp_lookup(uint32_t target);
tsfp_guest_entry_t recomp_lookup_kernel(uint32_t target);

static inline int tsfp_record_guest_call(uint32_t target) {
    g_icall_trace[g_icall_trace_idx & (ICALL_TRACE_SIZE - 1u)] = target;
    ++g_icall_trace_idx;
    ++g_icall_count;
    return RECOMP_ICALL_IS_CODE(target);
}

static inline tsfp_guest_entry_t tsfp_guest_target(uint32_t target) {
    tsfp_guest_entry_t resolved = recomp_lookup_manual(target);
    if (resolved) return resolved;
    resolved = recomp_lookup(target);
    return resolved ? resolved : recomp_lookup_kernel(target);
}

/* Diagnostics inspect the original call frame. They may also change guest
 * state, so failure recovery must run after the diagnostic returns. */
static inline int tsfp_invoke_guest_target(uint32_t target, uint32_t form) {
    tsfp_guest_entry_t resolved = tsfp_guest_target(target);
    if (!resolved) {
        RECOMP_ICALL_OBSERVE(target, RECOMP_ICALL_SEEN_UNRESOLVED);
        g_icall_saved_esp = g_esp;
        g_icall_dispatch_form = form;
        recomp_icall_fail_log(target);
        return 0;
    }
    RECOMP_ICALL_OBSERVE(target, RECOMP_ICALL_SEEN_RESOLVED);
    RECOMP_ABI_CALL(target, resolved);
    return 1;
}

#define TSFP_GUEST_CALL(target, report_stack, recover, observe_site) do { \
    const uint32_t tsfp_call_target_ = (uint32_t)(target); \
    if (tsfp_record_guest_call(tsfp_call_target_)) { \
        observe_site; \
        if (!tsfp_invoke_guest_target(tsfp_call_target_, 1u)) { \
            recover; \
            eax = 0; \
        } \
    } else { \
        recomp_icall_not_code_log(tsfp_call_target_, (report_stack)); \
        recover; \
        eax = 0; \
    } \
} while (0)
/* TSFP indirect dispatch support end. */

#define RECOMP_ICALL(xbox_va) \
    TSFP_GUEST_CALL(xbox_va, g_esp + 4u, g_esp += 4u, (void)0)


/* TSFP HLE frames begin.
 * Register-argument thunks stage at most 24 words in ordinary guest stack
 * memory. Keep the caller's return address and entry stack separate from any
 * register changes made while evaluating cleanup, selecting or calling HLE.
 */
#define RECOMP_HLE_MAX_ARGS 24u
typedef struct {
    uint32_t count, return_address, arguments, entry, work, cleanup;
} tsfp_hle_frame;

static inline tsfp_hle_frame tsfp_begin_hle_frame(uint32_t count) {
    tsfp_hle_frame frame;
    frame.count = count;
    frame.return_address = MEM32(g_esp);
    frame.arguments = g_esp + 4u;
    frame.entry = g_esp;
    frame.work = g_esp - 4u * (count + 4u);
    frame.cleanup = 0;
    return frame;
}

static inline void tsfp_run_hle_frame(const tsfp_hle_frame *frame,
                                      const uint32_t *arguments, void (*target)(void)) {
    uint32_t index = 0;
    MEM32(frame->work) = frame->return_address;
    while (index < frame->count && index < RECOMP_HLE_MAX_ARGS) {
        MEM32(frame->work + 4u * (index + 1u)) = arguments[index];
        ++index;
    }
    g_esp = frame->work;
    target();
    g_esp = frame->entry + frame->cleanup;
}

#define RECOMP_HLE_STACK(index) MEM32(tsfp_hle_frame_.arguments + 4u * (uint32_t)(index))
#define RECOMP_HLE_LTCG_CALL(n, fn, total_pop) { \
    tsfp_hle_frame tsfp_hle_frame_ = tsfp_begin_hle_frame((uint32_t)(n)); \
    tsfp_hle_frame_.cleanup = (uint32_t)(total_pop); \
    uint32_t a[RECOMP_HLE_MAX_ARGS]; \
    void (*tsfp_hle_target_)(void) = (fn);
#define RECOMP_HLE_LTCG_END \
    tsfp_run_hle_frame(&tsfp_hle_frame_, a, tsfp_hle_target_); \
}
/* TSFP HLE frames end. */


/* TSFP indirect dispatch wrappers begin.
 * A saved stack is consulted only for failure. For an invalid address it is
 * consulted before the diagnostic and again afterwards, as in the public ABI.
 * Tail jumps reuse the existing frame and do not enter the call trace ring.
 */
#define RECOMP_ICALL_SAFE(xbox_va, saved_esp) \
    TSFP_GUEST_CALL(xbox_va, saved_esp, g_esp = (saved_esp), (void)0)
#define RECOMP_ICALL_SAFE_AT(xbox_va, saved_esp, site) \
    TSFP_GUEST_CALL(xbox_va, saved_esp, g_esp = (saved_esp), \
        RECOMP_ICALL_OBSERVE_SITE((site), tsfp_call_target_))
#define RECOMP_ITAIL(xbox_va) do { \
    const uint32_t tsfp_tail_target_ = (uint32_t)(xbox_va); \
    if (!tsfp_invoke_guest_target(tsfp_tail_target_, 2u)) { \
        g_esp += 4u; \
        g_eax = 0; \
    } \
} while (0)
/* TSFP indirect dispatch wrappers end. */

/* New generator files declare this protocol before including the header.
 * Legacy/manual sources keep direct global aliases. References point at the
 * same thread-local cells: no register values are copied, delayed or flushed. */
#ifndef RECOMP_GPR_ANCHORS
#define RECOMP_GPR_ANCHORS(site) 0
#endif
#if defined(RECOMP_GENERATED_CODE) && defined(RECOMP_GPR_REFS_SUPPORTED) && \
    defined(RECOMP_USE_GPR_REFS) && RECOMP_USE_GPR_REFS
#define RECOMP_DECLARE_GPRS(site) \
    const int _recomp_use_refs = !!RECOMP_GPR_ANCHORS(site); \
    uint32_t *const _recomp_eax=&g_eax; (void)_recomp_eax; \
    uint32_t *const _recomp_ecx=&g_ecx; (void)_recomp_ecx; \
    uint32_t *const _recomp_edx=&g_edx; (void)_recomp_edx; \
    uint32_t *const _recomp_esp=&g_esp; (void)_recomp_esp; \
    uint32_t *const _recomp_ebx=&g_ebx; (void)_recomp_ebx; \
    uint32_t *const _recomp_esi=&g_esi; (void)_recomp_esi; \
    uint32_t *const _recomp_edi=&g_edi; (void)_recomp_edi; \
    RecompXmm *const _recomp_xmm0=&g_xmm0; (void)_recomp_xmm0; \
    RecompXmm *const _recomp_xmm1=&g_xmm1; (void)_recomp_xmm1; \
    RecompXmm *const _recomp_xmm2=&g_xmm2; (void)_recomp_xmm2; \
    RecompXmm *const _recomp_xmm3=&g_xmm3; (void)_recomp_xmm3; \
    RecompXmm *const _recomp_xmm4=&g_xmm4; (void)_recomp_xmm4; \
    RecompXmm *const _recomp_xmm5=&g_xmm5; (void)_recomp_xmm5; \
    RecompXmm *const _recomp_xmm6=&g_xmm6; (void)_recomp_xmm6; \
    RecompXmm *const _recomp_xmm7=&g_xmm7; (void)_recomp_xmm7; \
    (void)_recomp_use_refs
#define eax (*(_recomp_use_refs ? _recomp_eax : &g_eax))
#define ecx (*(_recomp_use_refs ? _recomp_ecx : &g_ecx))
#define edx (*(_recomp_use_refs ? _recomp_edx : &g_edx))
#define esp (*(_recomp_use_refs ? _recomp_esp : &g_esp))
#define ebx (*(_recomp_use_refs ? _recomp_ebx : &g_ebx))
#define esi (*(_recomp_use_refs ? _recomp_esi : &g_esi))
#define edi (*(_recomp_use_refs ? _recomp_edi : &g_edi))
#else
#define RECOMP_DECLARE_GPRS(site) ((void)0)
#ifdef RECOMP_GENERATED_CODE
#define eax g_eax
#define ecx g_ecx
#define edx g_edx
#define esp g_esp
#define ebx g_ebx
#define esi g_esi
#define edi g_edi
#endif
#endif

/* Emitted functions use architectural names; EBP stays function-local. */
#ifdef RECOMP_GENERATED_CODE
#define xmm0 g_xmm0
#define xmm1 g_xmm1
#define xmm2 g_xmm2
#define xmm3 g_xmm3
#define xmm4 g_xmm4
#define xmm5 g_xmm5
#define xmm6 g_xmm6
#define xmm7 g_xmm7
#define mm0 g_mm0
#define mm1 g_mm1
#define mm2 g_mm2
#define mm3 g_mm3
#define mm4 g_mm4
#define mm5 g_mm5
#define mm6 g_mm6
#define mm7 g_mm7
#endif

#endif /* RECOMP_TYPES_H */

/* Intentionally outside the type guard: generated function prefixes reinclude
 * this header with a site. Unselected bodies retain their direct aliases before
 * optimization, avoiding conditional vector-expression growth in large code. */
#if defined(RECOMP_GENERATED_CODE) && defined(RECOMP_REGISTER_SITE)
#undef xmm0
#undef xmm1
#undef xmm2
#undef xmm3
#undef xmm4
#undef xmm5
#undef xmm6
#undef xmm7
#if defined(RECOMP_GPR_REFS_SUPPORTED) && defined(RECOMP_USE_GPR_REFS) && RECOMP_USE_GPR_REFS && RECOMP_GPR_ANCHORS(RECOMP_REGISTER_SITE)
#define xmm0 (*_recomp_xmm0)
#define xmm1 (*_recomp_xmm1)
#define xmm2 (*_recomp_xmm2)
#define xmm3 (*_recomp_xmm3)
#define xmm4 (*_recomp_xmm4)
#define xmm5 (*_recomp_xmm5)
#define xmm6 (*_recomp_xmm6)
#define xmm7 (*_recomp_xmm7)
#else
#define xmm0 g_xmm0
#define xmm1 g_xmm1
#define xmm2 g_xmm2
#define xmm3 g_xmm3
#define xmm4 g_xmm4
#define xmm5 g_xmm5
#define xmm6 g_xmm6
#define xmm7 g_xmm7
#endif
#undef RECOMP_REGISTER_SITE
#endif
