#include <math.h>
/* US 43220: preserve the raw interval across its per-controller exit/wait
 * loop. State 2 overwrites the guest raw float with one while visiting the
 * first controller; subsequent controllers must still use this frame's time.
 * Playback packet cadence remains separate from the recorder below. */
static RECOMP_MANUAL_TLS float pc_native_demo_raw_interval;
static RECOMP_MANUAL_TLS int pc_native_demo_interval_active;
static RECOMP_MANUAL_TLS int pc_native_demo_exit_seen;
static RECOMP_MANUAL_TLS int pc_native_demo_skip_record;
static RECOMP_MANUAL_TLS unsigned pc_native_demo_generation;
static RECOMP_MANUAL_TLS struct {
    uint32_t base,size,cursor,channels,game,start,next;
    int valid;
} pc_native_demo_record;

/* Original initialization resets the cursor even when reusing the same
 * buffer. Observe that store, rather than guessing restart from pointer reuse. */
uint32_t pc_native_demo_cursor_reset(uint32_t value)
{
    pc_native_demo_record.valid=0;
    pc_native_demo_skip_record=0;
    ++pc_native_demo_generation;
    return value;
}

uint32_t pc_native_demo_dispatch_mode(uint32_t value)
{
    /* Skip only serialization. Original device polling, cancellation and
     * lifecycle services still execute on every native update. */
    return value==3u && pc_native_demo_skip_record ? 0u : value;
}

#define ND_WORD(a) (*(uint32_t *)((uintptr_t)(a)+g_xbox_mem_offset))
static int pc_native_demo_record_begin(int active,uint32_t mode,uint32_t ticks)
{
    if(mode!=3u || (!active && !pc_native_demo_record.valid)) {
        pc_native_demo_record.valid=0;return 0;
    }
    uint32_t base=ND_WORD(0x6B989Cu),size=ND_WORD(0x6B98A0u);
    uint32_t cursor=ND_WORD(0x6B94C0u),channels=ND_WORD(0x6B98A8u);
    uint32_t game=ND_WORD(0x4E7944u),level=ND_WORD(0x7DE318u);
    uint64_t end=(uint64_t)base+size;
    if(!base || channels<1 || channels>4 || end>UINT32_MAX ||
       cursor<base || cursor>=end || (cursor-base)%(14u*channels) ||
       ticks>255u || game>255u) {
        pc_native_demo_record.valid=0;return 0;
    }
    if(!pc_native_demo_record.valid || pc_native_demo_record.base!=base ||
       pc_native_demo_record.size!=size || pc_native_demo_record.cursor!=cursor ||
       pc_native_demo_record.channels!=channels || pc_native_demo_record.next!=level) {
        pc_native_demo_record.base=base;pc_native_demo_record.size=size;
        pc_native_demo_record.cursor=cursor;pc_native_demo_record.channels=channels;
        pc_native_demo_record.game=0;pc_native_demo_record.start=level;
        pc_native_demo_record.valid=1;
    }
    if(game>255u-pc_native_demo_record.game) {
        pc_native_demo_record.valid=0;return 0;
    }
    pc_native_demo_record.game+=game;
    pc_native_demo_record.next=level+game;
    return 1;
}

static void pc_native_demo_record_end(int active,unsigned generation)
{
    if(!pc_native_demo_record.valid || generation!=pc_native_demo_generation)return;
    uint32_t cursor=ND_WORD(0x6B94C0u),begin=pc_native_demo_record.cursor;
    uint32_t bytes=cursor-begin;
    if(ND_WORD(0x6B989Cu)!=pc_native_demo_record.base ||
       ND_WORD(0x6B98A0u)!=pc_native_demo_record.size ||
       ND_WORD(0x6B98A8u)!=pc_native_demo_record.channels ||
       cursor<begin || bytes>14u*pc_native_demo_record.channels || bytes%14u ||
       (uint64_t)cursor>(uint64_t)pc_native_demo_record.base+pc_native_demo_record.size) {
        pc_native_demo_record.valid=0;return;
    }
    /* Only touch records the original writer just wrote, with unchanged
     * buffer identity and bounded cursor advance. Game ticks can cross on a
     * different frame from raw ticks after pause: retain those pending ticks
     * and their starting timestamp, without changing live simulation globals. */
    for(uint32_t offset=0;offset<bytes;offset+=14u) {
        unsigned char *packet=(unsigned char *)((uintptr_t)(begin+offset)+g_xbox_mem_offset);
        packet[1]=(unsigned char)pc_native_demo_record.game;
        packet[2]=(unsigned char)pc_native_demo_record.start;
        packet[3]=(unsigned char)(pc_native_demo_record.start>>8);
    }
    if(bytes) {
        pc_native_demo_record.game=0;
        pc_native_demo_record.start=ND_WORD(0x7DE318u)+ND_WORD(0x4E7944u);
        pc_native_demo_record.cursor=cursor;
    }
    if(!active || ND_WORD(0x6B98A4u)!=3u)pc_native_demo_record.valid=0;
}
#undef ND_WORD

float pc_native_demo_timer_multiply(uint32_t site, float left, float right)
{
    if(pc_native_demo_interval_active && site==0x43473u) pc_native_demo_exit_seen=1;
    return left * (pc_native_demo_interval_active ? pc_native_demo_raw_interval : right);
}

static void pc_native_demo_guest(void (*original)(void))
{
    uint32_t *raw_ticks=(uint32_t *)((uintptr_t)0x4E7940u+g_xbox_mem_offset);
    float *raw_delta=(float *)((uintptr_t)0x4E7948u+g_xbox_mem_offset);
    uint32_t *mode=(uint32_t *)((uintptr_t)0x6B98A4u+g_xbox_mem_offset);
    float previous_interval=pc_native_demo_raw_interval, interval=*raw_delta;
    int previous_active=pc_native_demo_interval_active;
    int previous_exit=pc_native_demo_exit_seen;
    int previous_skip=pc_native_demo_skip_record;
    uint32_t ticks=*raw_ticks, entry_mode=*mode;
    int active=g_tsfp_native_timing_active && isfinite(interval) && interval>=0.f;
    pc_native_demo_interval_active=active;
    pc_native_demo_raw_interval=interval;
    pc_native_demo_exit_seen=0;
    int recording=pc_native_demo_record_begin(active,entry_mode,ticks);
    unsigned generation=pc_native_demo_generation;
    pc_native_demo_skip_record=recording && active && !ticks;
    original();
    if(recording)pc_native_demo_record_end(active,generation);
    /* Exit cleanup can reinitialize the level and replace the demo mode.
     * Remember the executed exit site as well as the entry/EOF state. Keep
     * its game pause while UI/audio raw time stays at native pace. */
    if(active && (entry_mode==2u || pc_native_demo_exit_seen || (entry_mode==1u && *mode==2u))) {
        *raw_ticks=ticks;
        *raw_delta=interval;
    }
    pc_native_demo_raw_interval=previous_interval;
    pc_native_demo_interval_active=previous_active;
    pc_native_demo_exit_seen=previous_exit;
    pc_native_demo_skip_record=previous_skip;
}
