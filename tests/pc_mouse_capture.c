#include <windows.h>
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)

static HWND foreground=(HWND)1;
static RECT client={0,0,2560,1440}, actual;
static POINT origin={-2560,120};
static int valid_client=1,valid_origin=1,valid_clip=1,clip_calls;
static BOOL fake_client(HWND window,RECT *rect) { (void)window;*rect=client;return valid_client; }
static BOOL fake_origin(HWND window,POINT *point) { (void)window;*point=origin;return valid_origin; }
static HWND fake_foreground(void) { return foreground; }
static BOOL fake_get_clip(RECT *rect) { *rect=actual;return valid_clip; }
static BOOL fake_clip(const RECT *rect) { actual=*rect;++clip_calls;return TRUE; }
#define GetClientRect fake_client
#define ClientToScreen fake_origin
#define GetForegroundWindow fake_foreground
#define GetClipCursor fake_get_clip
#define ClipCursor fake_clip
#include "pc_mouse_capture.h"

int main(void)
{
    HWND window=(HWND)1;
    clip_to_window(window);
    CHECK(clip_calls==1);
    CHECK(actual.left==-1280 && actual.top==840);
    CHECK(actual.right==actual.left+1 && actual.bottom==actual.top+1);
    restore_window_clip(window);CHECK(clip_calls==1);
    /* A launcher/focus transition or another component can clear the global
       clip. Repeated repairs must restore the same point, never a border. */
    for(int i=0;i<32;i++) {
        actual=(RECT){-4000,-4000,4000,4000};
        restore_window_clip(window);
        CHECK(actual.left==-1280 && actual.top==840);
        CHECK(actual.right==-1279 && actual.bottom==841);
    }
    /* Window move/resize and a borderless monitor-sized client. */
    client=(RECT){0,0,1920,1080};origin=(POINT){0,0};
    clip_to_window(window);
    CHECK(actual.left==960 && actual.top==540 && actual.right==961 && actual.bottom==541);
    int before=clip_calls;
    valid_clip=0;restore_window_clip(window);valid_clip=1;
    CHECK(clip_calls==before+1); /* failed query still attempts recovery */
    before=clip_calls;
    foreground=(HWND)2;clip_to_window(window);restore_window_clip(window);
    CHECK(clip_calls==before); /* never steal another application's clip */
    foreground=window;
    valid_client=0;clip_to_window(window);restore_window_clip(window);
    CHECK(clip_calls==before);valid_client=1;
    valid_origin=0;clip_to_window(window);restore_window_clip(window);
    CHECK(clip_calls==before);valid_origin=1;
    client.right=0;clip_to_window(window);restore_window_clip(window);
    CHECK(clip_calls==before);
    client.right=1920;client.bottom=0;
    clip_to_window(window);restore_window_clip(window);CHECK(clip_calls==before);
    clip_to_window(NULL);restore_window_clip(NULL);CHECK(clip_calls==before);
    puts("PASS: centered Win32 confinement, repair, move/resize and foreground/error guards");
    return 0;
}
