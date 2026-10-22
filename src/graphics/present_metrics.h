#ifndef TSFP_PRESENT_METRICS_H
#define TSFP_PRESENT_METRICS_H
/* One recorder for all host APIs. Call on the render thread after Present/Swap.
 * Counters represent API acceptance, never GPU completion or monitor scanout.
 * kind uses the RHI order: other=0, guest=1, interpolated=2. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#define TSFP_PRESENT_PROFILE_LIMIT 20000u

typedef struct {
    uint64_t total, guest, interpolated, other, skipped, failed;
    uint64_t first_ns, last_ns;
    unsigned checked, rows;
    FILE *output;
} TsfpPresentMetrics;

static void tsfp_present_record(TsfpPresentMetrics *m,const char *backend,unsigned kind,
                                int32_t result,uint64_t start_ns,uint64_t end_ns)
{
    if(result>0){m->skipped++;return;}
    if(result<0){m->failed++;return;}
    m->total++;
    if(kind==1)m->guest++;
    else if(kind==2)m->interpolated++;
    else m->other++;
    if(m->total==1 || m->total%300==0) {
        fprintf(stderr,"[PRESENT] backend=%s total=%" PRIu64 " guest=%" PRIu64
            " interpolated=%" PRIu64 " other=%" PRIu64 " skipped=%" PRIu64 " failed=%" PRIu64 "\n",
            backend,m->total,m->guest,m->interpolated,m->other,m->skipped,m->failed);
    }
    if(!m->checked) {
        const char *path=getenv("TSFP_FRAME_PROFILE");
        if(!path || !*path)path=getenv("TSFP_NATIVE_FRAME_PROFILE");
        m->checked=1;
        if(path && *path) {
            m->output=fopen(path,"w");
            /* Raw host clock, not UTC. Windows RHI uses QPC, allowing exact
             * alignment with the isolated runner's perf_counter_ns markers. */
            if(m->output)fputs("frame,elapsed_ms,frame_ms,swap_ms,kind,guest_frame,clock_ns\n",m->output);
        }
    }
    if(m->total==1)m->first_ns=end_ns;
    /* Failed/skipped attempts do not update last_ns: the next interval includes
     * the real gap. Omit the first timestamp because it has no prior interval. */
    if(m->output && m->total>1 && end_ns>m->last_ns && end_ns>=start_ns) {
        fprintf(m->output,"%" PRIu64 ",%.3f,%.3f,%.3f,%s,%" PRIu64 ",%" PRIu64 "\n",m->total,
            (end_ns-m->first_ns)/1e6,(end_ns-m->last_ns)/1e6,(end_ns-start_ns)/1e6,
            kind==1?"guest":kind==2?"interpolated":"other",m->guest,end_ns);
        if(++m->rows%60==0)fflush(m->output);
        if(m->rows==TSFP_PRESENT_PROFILE_LIMIT){fclose(m->output);m->output=NULL;}
    }
    m->last_ns=end_ns;
}
#endif
