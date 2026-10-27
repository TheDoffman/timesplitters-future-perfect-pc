#ifndef TSFP_PC_HANG_POLICY_H
#define TSFP_PC_HANG_POLICY_H
#include <stdint.h>

typedef struct PcHangPolicy {
    uint64_t last_poll, last_progress, sequence;
    int armed, reported;
} PcHangPolicy;

/* Clock gaps cover machine sleep/resume or a globally starved helper. Pausing
 * observation never rearms a report; only actual frame progress does that. */
static int pc_hang_poll(PcHangPolicy *p, uint64_t now, uint64_t sequence,
    int armed, int suppressed, uint32_t frame_timeout, uint32_t startup_timeout)
{
    if (!armed) { p->armed = 0; p->last_poll = now; return 0; }
    if (!p->armed) {
        p->armed = 1; p->reported = 0; p->sequence = sequence; p->last_progress = now;
    }
    if (now < p->last_poll || now - p->last_poll > 5000) p->last_progress = now;
    p->last_poll = now;
    if (sequence != p->sequence) {
        p->sequence = sequence; p->last_progress = now; p->reported = 0;
    }
    if (suppressed) p->last_progress = now;
    uint32_t timeout = sequence ? frame_timeout : startup_timeout;
    if (!suppressed && !p->reported && now - p->last_progress >= timeout) {
        p->reported = 1; return 1;
    }
    return 0;
}
#endif
