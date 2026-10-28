/* Project recorder for the branch-feedback v1 text protocols.
 * Target flags are atomic; site sets use a bounded chained table. File output
 * is best effort, not a transaction or an async-signal-safe crash facility.
 */
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "tsfp_branch_feedback.h"
#ifdef RECOMP_ICALL_FEEDBACK
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#define TSFP_FEEDBACK_TLS __declspec(thread)
static int32_t swap32(volatile int32_t *p, int32_t value) {
    return (int32_t)_InterlockedExchange((volatile long *)p, (long)value);
}
static int32_t claim32(volatile int32_t *p) {
    return (int32_t)_InterlockedCompareExchange((volatile long *)p, 1, 0);
}
static void or8(volatile unsigned char *p, unsigned char value) {
    (void)_InterlockedOr8((volatile char *)p, (char)value);
}
static unsigned char read8(volatile unsigned char *p) {
    return (unsigned char)_InterlockedCompareExchange8((volatile char *)p, 0, 0);
}
static void increment64(volatile uint64_t *p) {
    (void)_InterlockedIncrement64((volatile __int64 *)p);
}
static uint64_t read64(volatile uint64_t *p) {
    return (uint64_t)_InterlockedCompareExchange64((volatile __int64 *)p, 0, 0);
}
#else
#define TSFP_FEEDBACK_TLS __thread
static int32_t swap32(volatile int32_t *p, int32_t value) {
    return __atomic_exchange_n(p, value, __ATOMIC_SEQ_CST);
}
static int32_t claim32(volatile int32_t *p) {
    int32_t expected=0;
    (void)__atomic_compare_exchange_n(p, &expected, 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return expected;
}
static void or8(volatile unsigned char *p, unsigned char value) {
    (void)__atomic_fetch_or(p, value, __ATOMIC_RELAXED);
}
static unsigned char read8(volatile unsigned char *p) {
    return __atomic_load_n(p, __ATOMIC_RELAXED);
}
static void increment64(volatile uint64_t *p) {
    (void)__atomic_fetch_add(p, UINT64_C(1), __ATOMIC_RELAXED);
}
static uint64_t read64(volatile uint64_t *p) {
    return __atomic_load_n(p, __ATOMIC_RELAXED);
}
#endif

enum { FEEDBACK_BUCKETS=4096, FEEDBACK_SITES=32768, FEEDBACK_TARGETS=6 };
typedef struct {
    uint32_t site, next, target[FEEDBACK_TARGETS];
    unsigned count, saturated;
} feedback_site;
static volatile unsigned char observations[RECOMP_ICALL_FB_SIZE];
static feedback_site sites[FEEDBACK_SITES];
static uint32_t buckets[FEEDBACK_BUCKETS];
static unsigned used_sites;
static uint64_t dropped_sites;
static volatile int32_t site_lock, exit_registered;
static TSFP_FEEDBACK_TLS uint32_t last_site, last_target;
volatile uint64_t g_icall_guard_hits, g_icall_guard_misses;

const char *tsfp_feedback_output_path(void) {
    const char *path=getenv("RECOMP_ICALL_FEEDBACK_PATH");
    return path && *path ? path : RECOMP_ICALL_FEEDBACK_PATH;
}
static void feedback_on_exit(void) {
    recomp_icall_feedback_dump(tsfp_feedback_output_path());
}
void recomp_icall_feedback_init(void) {
    if (claim32(&exit_registered)==0 && atexit(feedback_on_exit)!=0)
        (void)swap32(&exit_registered,0);
}

void tsfp_feedback_target(uint32_t address, unsigned char flags) {
    if (!tsfp_feedback_accepts(address)) return;
    or8(&observations[address-RECOMP_ICALL_FB_BASE],flags);
}
void tsfp_feedback_guard(unsigned matched) {
    increment64(matched ? &g_icall_guard_hits : &g_icall_guard_misses);
}

void recomp_icall_observe_site(uint32_t site, uint32_t target) {
    if (!site || (site==last_site && target==last_target)) return;
    const uint32_t bucket=(site ^ (site>>12) ^ (site>>24)) & (FEEDBACK_BUCKETS-1u);
    while (claim32(&site_lock)!=0) { }
    uint32_t link=buckets[bucket];
    while (link && sites[link-1u].site!=site) link=sites[link-1u].next;
    if (!link) {
        if (used_sites==FEEDBACK_SITES) {
            ++dropped_sites;
            (void)swap32(&site_lock,0);
            return;
        }
        link=++used_sites;
        sites[link-1u].site=site;
        sites[link-1u].next=buckets[bucket];
        buckets[bucket]=link;
    }
    feedback_site *record=&sites[link-1u];
    unsigned index=0;
    while (index<record->count && record->target[index]!=target) ++index;
    if (index==record->count) {
        if (record->count<FEEDBACK_TARGETS) record->target[record->count++]=target;
        else record->saturated=1;
    }
    (void)swap32(&site_lock,0);
    last_site=site;last_target=target;
}

static char *companion_path(const char *path) {
    static const char suffix[]="targets.dump";
    size_t length=strlen(path), prefix=length;
    const char *tail=".sites";
    if (length>=sizeof(suffix)-1u &&
        strcmp(path+length-(sizeof(suffix)-1u),suffix)==0) {
        prefix=length-(sizeof(suffix)-1u);tail="sites.dump";
    }
    size_t ending=strlen(tail)+1u;
    if (prefix>SIZE_MAX-ending) return NULL;
    char *result=(char *)malloc(prefix+ending);
    if (result) { memcpy(result,path,prefix);memcpy(result+prefix,tail,ending); }
    return result;
}

static void write_sites(const char *targets_path) {
    feedback_site *snapshot=(feedback_site *)malloc(sizeof(sites));
    if (!snapshot) return;
    /* A crash callback must not spin on a lock its interrupted thread owns. */
    if (claim32(&site_lock)!=0) {
        free(snapshot);
        fprintf(stderr,"[tsfp-feedback] site snapshot busy; previous dump retained\n");
        return;
    }
    unsigned count=used_sites;
    uint64_t dropped=dropped_sites;
    memcpy(snapshot,sites,count*sizeof(*snapshot));
    (void)swap32(&site_lock,0);
    char *path=companion_path(targets_path);
    FILE *output=path ? fopen(path,"w") : NULL;
    unsigned saturated=0;
    if (output) {
        fputs("# icall-sites v1\n",output);
        for (unsigned i=0;i<count;++i) {
            feedback_site *record=&snapshot[i];
            fprintf(output,"%08X",(unsigned)record->site);
            for (unsigned k=0;k<record->count;++k)
                fprintf(output," %08X",(unsigned)record->target[k]);
            if (record->saturated) { fputs(" +",output);++saturated; }
            fputc('\n',output);
        }
        fprintf(output,"# dropped site observations: %llu\n",(unsigned long long)dropped);
        int failed=ferror(output);
        if (fclose(output)!=0) failed=1;
        if (failed) fprintf(stderr,"[tsfp-feedback] incomplete site dump: %s\n",path);
        fprintf(stderr,"[tsfp-feedback] %u sites, %u saturated, %llu dropped; guards %llu/%llu\n",
                count,saturated,(unsigned long long)dropped,
                (unsigned long long)read64(&g_icall_guard_hits),
                (unsigned long long)read64(&g_icall_guard_misses));
    }
    free(path);free(snapshot);
}

void recomp_icall_feedback_dump(const char *path) {
    if (!path || !*path) return;
    FILE *output=fopen(path,"w");
    if (!output) { fprintf(stderr,"[tsfp-feedback] cannot open target dump: %s\n",path);return; }
    fputs("# icall-feedback v1\n",output);
    unsigned resolved=0,unresolved=0;
    for (uint32_t offset=0;offset<RECOMP_ICALL_FB_SIZE;++offset) {
        unsigned char flags=read8(&observations[offset]);
        if (!flags) continue;
        fprintf(output,"%08X %u\n",(unsigned)(offset+RECOMP_ICALL_FB_BASE),(unsigned)flags);
        resolved+=(flags & RECOMP_ICALL_SEEN_RESOLVED)!=0;
        unresolved+=(flags & RECOMP_ICALL_SEEN_UNRESOLVED)!=0;
    }
    int failed=ferror(output);
    if (fclose(output)!=0) failed=1;
    if (failed) fprintf(stderr,"[tsfp-feedback] incomplete target dump: %s\n",path);
    fprintf(stderr,"[tsfp-feedback] %u resolved, %u unresolved targets\n",resolved,unresolved);
    write_sites(path);
}
#else
typedef int tsfp_branch_feedback_disabled;
#endif
