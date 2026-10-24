/* Exercise the actual config parser and sampler with deterministic pad I/O. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "input_bindings.h"
#ifdef _WIN32
static SHORT test_key_state(int vk) { (void)vk; return 0; }
#define GetAsyncKeyState test_key_state
#endif
#include "input_bindings.c"

static RecompPadState state;
static int connected, selected_slot;
int recomp_pad_read(int slot, RecompPadState *out)
{
    memset(out,0,sizeof *out);
    if (!connected || slot!=selected_slot) return 0;
    *out=state; return 1;
}
void recomp_pad_name(int slot,char *out,size_t n) { (void)slot; snprintf(out,n,"Test pad"); }
void recomp_pad_rumble(int slot,uint16_t low,uint16_t high) { (void)slot;(void)low;(void)high; }
void recomp_pad_set_api(int api) { (void)api; }
const char *recomp_pad_api_name(void) { return "Test"; }

int main(int argc,char **argv)
{
    XBOX_GAMEPAD out, neutral={0};
    char *text;
    Binding saved[CONTROL_COUNT];
    assert(argc==4);
    selected_slot=atoi(argv[2]);
    text=read_file(argv[1]); assert(text);
    for(int i=0;i<PORTS;i++) defaults_for(&g_ctl[i],i);
    assert(parse_config(text)); free(text); g_ready=1;
    memcpy(saved,g_ctl[0].bind,sizeof saved);
    connected=1;
    state.buttons=RECOMP_PAD_A|RECOMP_PAD_START|RECOMP_PAD_DPAD_DOWN;
    state.lx=32767; state.ry=-32767; state.right_trigger=255;
    assert(recomp_bindings_sample(0,&out)==(strcmp(argv[3],"none")!=0));
    if (!strcmp(argv[3],"standard")) {
        assert(out.bAnalogButtons[XBOX_BUTTON_A]==255);
        assert(out.wButtons==(XBOX_GAMEPAD_START|XBOX_GAMEPAD_DPAD_DOWN));
        assert(out.bAnalogButtons[XBOX_BUTTON_RTRIGGER]==255);
        assert(out.sThumbLX==32767 && out.sThumbRY==-32767);
    } else if (!strcmp(argv[3],"custom")) {
        assert(out.bAnalogButtons[XBOX_BUTTON_A]==0); /* custom A=pad:B */
        assert(out.bAnalogButtons[XBOX_BUTTON_RTRIGGER]==0); /* explicitly unbound */
        state.buttons=RECOMP_PAD_B;
        assert(recomp_bindings_sample(0,&out));
        assert(out.bAnalogButtons[XBOX_BUTTON_A]==255);
    } else assert(!memcmp(&out,&neutral,sizeof out));
    memset(&state,0,sizeof state);
    recomp_bindings_sample(0,&out); assert(!memcmp(&out,&neutral,sizeof out));
    /* A different connected physical pad must not leak into the selected slot. */
    selected_slot=(selected_slot+1)%4; state.buttons=RECOMP_PAD_A;
    recomp_bindings_sample(0,&out); assert(!memcmp(&out,&neutral,sizeof out));
    connected=0;
    recomp_bindings_sample(0,&out); assert(!memcmp(&out,&neutral,sizeof out));
    assert(!memcmp(saved,g_ctl[0].bind,sizeof saved));
    puts("PASS: controller profile routing, release, disconnect and binding preservation");
    return 0;
}
