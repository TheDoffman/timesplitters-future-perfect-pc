/* Exercise real HLE playback lifetime with deterministic host-player stubs. */
#include "hle/hle_xmv.c"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static uint32_t memory[32768], next_object=0x2000;
ptrdiff_t g_xbox_mem_offset;
size_t g_xbox_total_ram=sizeof memory;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp;
static unsigned closes,starts,updates,fallbacks;
static int available=1,active;
static uint32_t host_status=1;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment)
{ uint32_t p=next_object; next_object+=bytes; CHECK(next_object<sizeof memory); return p; }
int xbox_EnvSwitch(const char *name,int default_on) { return default_on; }
int xmv_play_open(uint32_t source,uint32_t *w,uint32_t *h)
{ *w=640;*h=480;active=available;return available; }
void xmv_play_start(int handle) { if(handle) { CHECK(active); ++starts; } }
uint32_t xmv_play_update(int handle,uint32_t surface,uint32_t *pts)
{ CHECK(handle==1 && active);++updates;*pts=123;return host_status; }
uint32_t xmv_play_time(int handle) { return 123; }
void xmv_play_close(int handle) { if(handle) { CHECK(active);active=0;++closes; } }
static void original_stop(void) { ++fallbacks;g_eax=1234;g_esp+=8; }
void (*const hle_original_XMVPlaybackStop)(void)=original_stop;
#define NO_ORIGINAL(n) void (*const hle_original_##n)(void)=NULL
NO_ORIGINAL(XMVPlaybackStart);
NO_ORIGINAL(XMVPlaybackGetStreamInfo);
NO_ORIGINAL(XMVPlaybackCreateAudioStream);
NO_ORIGINAL(XMVPlaybackGetAudioStreamInfo);
NO_ORIGINAL(XMVPlaybackGetCurrentTime);
NO_ORIGINAL(XMVPlaybackUpdate);
NO_ORIGINAL(XMVPlaybackDestroy);
NO_ORIGINAL(XMVPlaybackReleaseAudioStream);
NO_ORIGINAL(XMVPlaybackStopAudioStreams);
NO_ORIGINAL(XMVPlaybackGetAudioStream);
static uint32_t create(void)
{
    HLE_MEM32(g_esp+4)=0;HLE_MEM32(g_esp+8)=0;HLE_MEM32(g_esp+12)=0x100;
    hle_XMVPlaybackCreate();CHECK(g_eax==0);
    return HLE_MEM32(0x100);
}
static void update(uint32_t obj,uint32_t expected)
{
    HLE_MEM32(g_esp+4)=obj;HLE_MEM32(g_esp+8)=0;
    HLE_MEM32(g_esp+12)=0x104;HLE_MEM32(g_esp+16)=0x10000;
    HLE_MEM32(0x104)=99;HLE_MEM32(0x10000)=456;
    hle_XMVPlaybackUpdate();CHECK(g_eax==0 && HLE_MEM32(0x104)==expected);
    CHECK(g_esp==0x1000);
    CHECK(HLE_MEM32(0x10000)==(expected==1 ? 124u : 456u));
}
int main(void)
{
    uint32_t obj;
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;g_esp=0x1000;
    obj=create();update(obj,1);CHECK(updates==1 && closes==0);
    hle_XMVPlaybackStop();CHECK(closes==1 && !active);
    CHECK(g_eax==0 && g_esp==0x1000 && starts==0);
    update(obj,2);update(obj,2);CHECK(updates==1);
    hle_XMVPlaybackStop();hle_XMVPlaybackDestroy();CHECK(closes==1);
    /* A later idle movie must still play and finish normally. */
    obj=create();update(obj,1);CHECK(updates==2 && active);
    host_status=2;update(obj,2);update(obj,2);CHECK(updates==3);
    hle_XMVPlaybackDestroy();CHECK(closes==2 && !active);
    /* Stop before the first poll, including unavailable/disabled host video. */
    for(available=1;available>=0;--available) {
        obj=create();HLE_MEM32(g_esp+4)=obj;
        hle_XMVPlaybackStop();update(obj,2);hle_XMVPlaybackDestroy();
    }
    CHECK(closes==3 && updates==3);
    /* Genuine Start semantics for other titles stay intact. */
    available=1;obj=create();HLE_MEM32(g_esp+4)=obj;
    hle_XMVPlaybackStart();CHECK(starts==1 && active);
    hle_XMVPlaybackStop();HLE_MEM32(g_esp+12)=0;
    hle_XMVPlaybackUpdate();CHECK(g_eax==0 && g_esp==0x1000);
    hle_XMVPlaybackDestroy();CHECK(closes==4);
    /* Non-HLE objects still call their original body, with thunk-owned stack. */
    HLE_MEM32(g_esp+4)=0x800;hle_XMVPlaybackStop();
    CHECK(fallbacks==1 && g_eax==1234 && g_esp==0x1000 && closes==4);
    puts("PASS: skip, EOF, repeated stop/destroy, next movie and original fallback");
    return 0;
}
