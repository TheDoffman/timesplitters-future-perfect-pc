#ifndef TSFP_PC_VIDEO_SIDEBAR_H
#define TSFP_PC_VIDEO_SIDEBAR_H
/* Values only: the presentation thread never calls into guest memory or saves
 * renderer settings. Requests are consumed between guest frames. */
enum { VIDEO_RATE,VIDEO_FOV,VIDEO_TEXTURE,VIDEO_AF,VIDEO_AO,VIDEO_AA,
       VIDEO_MSAA,VIDEO_SS,VIDEO_CACHE,VIDEO_SUBTITLES,VIDEO_FEED,VIDEO_COUNTER,
       VIDEO_FULLSCREEN,VIDEO_VALUE_COUNT };
enum { VIDEO_APPLY=1,VIDEO_KEEP,VIDEO_REVERT,VIDEO_CLEAR_CACHE,VIDEO_RESTART };
typedef struct PcVideoValues {
    unsigned width,height;
    int value[VIDEO_VALUE_COUNT];
} PcVideoValues;
typedef struct PcVideoSnapshot {
    PcVideoValues current;
    unsigned modes[128][2],mode_count;
    unsigned revision,completed;
    int available,effects,preview,restart;
    unsigned seconds;
    char message[160];
} PcVideoSnapshot;
typedef struct PcVideoRequest {unsigned serial;int action;PcVideoValues desired;} PcVideoRequest;
void pc_developer_video_publish(const PcVideoSnapshot *snapshot);
void pc_developer_video_snapshot(PcVideoSnapshot *snapshot);
int pc_developer_video_take(PcVideoRequest *request);
unsigned pc_developer_video_request(int action,const PcVideoValues *desired);
void pc_developer_video_register(void (*tick)(void));
void pc_developer_video_guest_tick(void);
#endif
