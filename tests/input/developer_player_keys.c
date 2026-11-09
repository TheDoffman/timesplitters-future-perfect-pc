/* Execute the real native gameplay guards and runtime keyboard dispatch. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pc_input.h"
typedef struct {unsigned char state[256];} KeyCache;
static unsigned char held[256];
static int focused=1,menu_active,freecam,overlay,capture;
#define atomic_load(p) (*(p))
static int pc_developer_open(void) {return overlay;}
static int pc_developer_blocks_player(void) {return freecam;}
static int pc_settings_capture_blocked(void) {return capture;}
static int tsfp_native_key_down(int key) {return focused&&key>0&&key<256&&held[key];}
/* Select the native adapter even when running this isolated fixture on Windows. */
#undef _WIN32
#define TSFP_NATIVE_INPUT
/* PRODUCTION_GUARDS */
int main(void) {
    KeyCache cache={0};held['W']=held[1]=1;
    assert(key_down(&cache,'W')&&key_down(&cache,1)&&pc_controls_accept_pad());
    freecam=1;
    /* Raw state must remain available to camera flight and binding capture. */
    assert(tsfp_native_key_down('W')&&tsfp_native_key_down(1));
    assert(!key_down(&cache,'W')&&!key_down(&cache,1)&&!pc_controls_accept_pad());
    freecam=0;
    assert(key_down(&cache,'W')&&key_down(&cache,1)&&pc_controls_accept_pad());
    overlay=1;
    assert(!key_down(&cache,'W')&&!pc_controls_accept_pad());
    overlay=0;capture=1;
    assert(!key_down(&cache,'W')&&!pc_controls_accept_pad());
    capture=0;focused=0;
    assert(!key_down(&cache,'W')&&!pc_controls_accept_pad());
    focused=1;menu_active=1;held['W']=1;held[0x26]=0;
    assert(key_down(&cache,0x26)); /* Preserve native menu aliases. */
    puts("PASS: native guest keys are neutral in freecam while raw camera keys remain held");
    return 0;
}
