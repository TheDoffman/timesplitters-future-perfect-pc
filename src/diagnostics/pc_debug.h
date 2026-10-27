#ifndef TSFP_PC_DEBUG_H
#define TSFP_PC_DEBUG_H
#ifdef _WIN32
#include <windows.h>
#else
typedef void *HWND;
#endif
#include <stddef.h>

#define PC_DEBUG_COMMAND 0x7e21
#define PC_DEBUG_TIMER 0x7e30
enum { PC_DEBUG_CAP_NETWORK=1u };
typedef enum PcDebugOptionId {
#define PC_DEBUG_OPTION(id,field,label,caps) PC_DEBUG_OPTION_##id,
#include "pc_debug_options.def"
#undef PC_DEBUG_OPTION
    PC_DEBUG_OPTION_COUNT
} PcDebugOptionId;
typedef struct PcDebugOptions {
#define PC_DEBUG_OPTION(id,field,label,caps) int field;
#include "pc_debug_options.def"
#undef PC_DEBUG_OPTION
} PcDebugOptions;
typedef struct PcDebugOption {
    const char *key,*label;
    unsigned capabilities;
    size_t offset;
} PcDebugOption;
static inline const PcDebugOption *pc_debug_option(unsigned index)
{
    static const PcDebugOption definitions[]={
#define PC_DEBUG_OPTION(id,field,label,caps) {#field,label,caps,offsetof(PcDebugOptions,field)},
#include "pc_debug_options.def"
#undef PC_DEBUG_OPTION
    };
    return index<PC_DEBUG_OPTION_COUNT?&definitions[index]:NULL;
}
static inline int pc_debug_option_available(unsigned index,unsigned capabilities)
{
    const PcDebugOption *option=pc_debug_option(index);
    return option && (option->capabilities & capabilities)==option->capabilities;
}
static inline unsigned pc_debug_option_count(unsigned capabilities)
{
    unsigned count=0;
    for(unsigned i=0;i<PC_DEBUG_OPTION_COUNT;i++)
        count+=pc_debug_option_available(i,capabilities)!=0;
    return count;
}
static inline int pc_debug_option_value(const PcDebugOptions *values,unsigned index)
{
    const PcDebugOption *option=pc_debug_option(index);
    return option?!!*(const int *)((const char *)values+option->offset):0;
}
static inline void pc_debug_option_set(PcDebugOptions *values,unsigned index,int value)
{
    const PcDebugOption *option=pc_debug_option(index);
    if(option)*(int *)((char *)values+option->offset)=!!value;
}
typedef struct PcDebugSnapshot {
    double fps, shown_fps, frame_ms;
    unsigned long errors, warnings, slow_frames;
    int log_available;
    int network_mode;
    double latency_ms;
    unsigned latency_peers;
    char recent[6144];
} PcDebugSnapshot;

/* Config access shares the controls file's parser and atomic save mechanism. */
void pc_debug_config_read(PcDebugOptions *options);
int pc_debug_config_save(const PcDebugOptions *options, char *error, size_t bytes);
void pc_debug_init(HWND window);
void pc_debug_shutdown(void);
void pc_debug_get(PcDebugOptions *options);
void pc_debug_set(const PcDebugOptions *options);
void pc_debug_toggle_fps(void);
void pc_debug_frame(void);
void pc_debug_sample(double fps, double shown_fps);
/* mode: 0 offline, 1 LAN, 2 Online. Negative RTT means no fresh measurement. */
void pc_debug_latency(int mode, double milliseconds, unsigned peers);
const char *pc_debug_watermark(void);
int pc_debug_overlay(char *text, size_t bytes, const char *cap);
void pc_debug_snapshot(PcDebugSnapshot *snapshot);
/* Log polling and clearing run on the window thread, never the render thread. */
void pc_debug_poll(void);
void pc_debug_clear(void);
int pc_debug_open_logs(HWND owner);
void pc_debug_show(HWND owner);
#endif
