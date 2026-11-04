/* Real SDL virtual-device lifecycle; no desktop focus or physical pads needed. */
#include <SDL3/SDL.h>
#include "recomp_pad.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s (%s)\n",__LINE__,#x,SDL_GetError());return 1;}}while(0)
static SDL_JoystickID attach(const char *name,SDL_Joystick **stick)
{
    SDL_VirtualJoystickDesc d={0};SDL_INIT_INTERFACE(&d);
    d.type=SDL_JOYSTICK_TYPE_GAMEPAD;d.naxes=SDL_GAMEPAD_AXIS_COUNT;d.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    d.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;d.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;d.name=name;
    SDL_JoystickID id=SDL_AttachVirtualJoystick(&d);*stick=SDL_OpenJoystick(id);return id;
}
int main(void)
{
    CHECK(SDL_Init(SDL_INIT_GAMEPAD));int count=0;SDL_JoystickID *ids=SDL_GetGamepads(&count);SDL_free(ids);
    if(count){puts("SKIP: real SDL lifecycle test requires no physical pads");return 0;}
    recomp_pad_set_api(RECOMP_PAD_API_SDL);
    SDL_Joystick *a,*b;SDL_JoystickID aid=attach("TSFP reconnect A",&a),bid=attach("TSFP reconnect B",&b);
    CHECK(aid && bid && a && b);RecompPadState p;SDL_Delay(300);
    CHECK(recomp_pad_read(0,&p) && recomp_pad_read(1,&p));
    CHECK(SDL_SetJoystickVirtualButton(b,SDL_GAMEPAD_BUTTON_START,true));SDL_Delay(2);
    CHECK(recomp_pad_read(1,&p) && (p.buttons&RECOMP_PAD_START));
    for(int cycle=0;cycle<6;cycle++) {
        SDL_CloseJoystick(a);CHECK(SDL_DetachVirtualJoystick(aid));SDL_Delay(300);
        CHECK(!recomp_pad_read(0,&p));CHECK(recomp_pad_read(1,&p) && (p.buttons&RECOMP_PAD_START));
        aid=attach("TSFP reconnect A",&a);CHECK(aid && a);SDL_Delay(300);
        CHECK(SDL_SetJoystickVirtualButton(a,SDL_GAMEPAD_BUTTON_SOUTH,true));SDL_Delay(2);
        CHECK(recomp_pad_read(0,&p) && (p.buttons&RECOMP_PAD_A));
        CHECK(recomp_pad_read(1,&p) && (p.buttons&RECOMP_PAD_START));
        CHECK(!recomp_pad_read(2,&p) && !recomp_pad_read(3,&p));
    }
    SDL_CloseJoystick(a);SDL_CloseJoystick(b);SDL_DetachVirtualJoystick(aid);SDL_DetachVirtualJoystick(bid);
    puts("PASS: six real SDL detach/reattach cycles restore input to the original slot; second controller stays independent");
    return 0;
}
