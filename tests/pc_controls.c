#include "pc_input.h"
#include "pc_camera.h"
#include "pc_controls.h"
#include "pc_text_input.h"
#include "input_bindings.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #test); return 1; } } while (0)
int main(int argc, char **argv)
{
    PcInput s = {0}, a = {0}, b = {0};
    float yaw = 0, pitch = 0, ya = 0, yb = 0, pa = 0, pb = 0;
    XBOX_GAMEPAD pad;
    char replay_file[MAX_PATH], state_file[MAX_PATH];
    FILE *f;
    unsigned char *guest;
    int i;
    ULONGLONG replay_epoch;
    LRESULT message_result = 0;
    int16_t mx = 32767, my = 32767;
    CHECK(argc == 3);
    CHECK(pc_controls_message(NULL, WM_MOUSEACTIVATE, 0, 0, &message_result));
    CHECK(message_result == MA_ACTIVATEANDEAT);
    pc_controls_menu_mode(1);
    CHECK(pc_controls_message(NULL, WM_SETCURSOR, 0, HTCLIENT, &message_result));
    CHECK(message_result == TRUE);
    CHECK(!pc_controls_message(NULL, WM_MOUSEACTIVATE, 0, 0, &message_result));
    pc_controls_menu_mode(0);
    /* Exercise the window adapter directly: Escape then menu exit must
       restore capture, while an unfocused window must stay uncaptured. */
    pc_controls_message(NULL,WM_SETFOCUS,0,0,&message_result);
    pc_controls_message(NULL,WM_KEYDOWN,VK_ESCAPE,0,&message_result);
    pc_controls_menu_mode(1);
    pc_controls_message(NULL,WM_APP+0x186,0,0,&message_result);
    pc_controls_menu_mode(0);
    pc_controls_message(NULL,WM_APP+0x186,1,0,&message_result);
    CHECK(pc_controls_message(NULL,WM_SETCURSOR,0,HTCLIENT,&message_result));
    pc_controls_message(NULL,WM_KILLFOCUS,0,0,&message_result);
    pc_controls_menu_mode(1);pc_controls_menu_mode(0);
    pc_controls_message(NULL,WM_APP+0x186,1,0,&message_result);
    CHECK(!pc_controls_message(NULL,WM_SETCURSOR,0,HTCLIENT,&message_result));
    /* Direct host-adapter events, without desktop input or personal settings. */
    pc_controls_message(NULL,WM_SETFOCUS,0,0,&message_result);
    pc_controls_text_mode(0x1000);
    CHECK(pc_controls_message(NULL,WM_KEYDOWN,'E',0,&message_result));
    CHECK(!pc_controls_key_down('E'));
    CHECK(pc_controls_message(NULL,WM_CHAR,'E',0,&message_result));
    CHECK(pc_controls_text_event(0x1000)=='E');
    CHECK(pc_controls_message(NULL,WM_CHAR,0xe9,0,&message_result));
    CHECK(pc_controls_text_event(0x1000)==0xe9);
    pc_controls_message(NULL,WM_KEYDOWN,VK_BACK,0,&message_result);
    pc_controls_message(NULL,WM_CHAR,8,0,&message_result);
    CHECK(pc_controls_text_event(0x1000)==PC_TEXT_BACKSPACE && !pc_controls_text_event(0x1000));
    pc_controls_message(NULL,WM_KEYDOWN,VK_RETURN,0,&message_result);
    pc_controls_message(NULL,WM_KEYDOWN,VK_RETURN,(LPARAM)1<<30,&message_result);
    CHECK(pc_controls_text_event(0x1000)==PC_TEXT_ACCEPT && !pc_controls_text_event(0x1000));
    pc_controls_message(NULL,WM_CHAR,'x',0,&message_result);
    pc_controls_message(NULL,WM_KILLFOCUS,0,0,&message_result);
    CHECK(!pc_controls_text_event(0x1000));
    pc_controls_message(NULL,WM_CHAR,'x',0,&message_result);
    CHECK(!pc_controls_text_event(0x1000));
    pc_controls_message(NULL,WM_SETFOCUS,0,0,&message_result);
    pc_controls_message(NULL,WM_CHAR,'x',0,&message_result);
    pc_controls_text_mode(0x2000);CHECK(!pc_controls_text_event(0x2000));
    pc_controls_text_mode(0);CHECK(!pc_controls_message(NULL,WM_CHAR,'x',0,&message_result));
    pc_controls_message(NULL,WM_KILLFOCUS,0,0,&message_result);
    pc_input_normalize_movement(&mx, &my);
    CHECK(mx == my && hypot(mx, my) <= 32767 && mx > 23000);
    mx = 0; my = -32767; pc_input_normalize_movement(&mx, &my);
    CHECK(mx == 0 && my == -32767);
    pc_input_focus(&s, 1); pc_input_capture(&s, 1);
    pc_input_key(&s, 'W', 1); CHECK(pc_input_down(&s, 'W', 0));
    pc_input_focus(&s, 0); CHECK(!pc_input_down(&s, 'W', 0));
    CHECK(!s.captured);
    pc_input_motion(&s, 100, 100, 100); CHECK(!s.dx && !s.dy);
    pc_input_focus(&s, 1); pc_input_capture(&s, 1);
    pc_input_motion(&s, 100, 20, 200);
    CHECK(pc_input_look(&s, 201, .1f, 0, 1, &yaw, &pitch));
    CHECK(fabs(yaw + 10) < .001 && fabs(pitch + 2) < .001);
    CHECK(!pc_input_look(&s, 202, .1f, 0, 1, &yaw, &pitch));
    pc_input_motion(&s, 0, 20, 210);
    CHECK(pc_input_look(&s, 211, .1f, 1, 1, &yaw, &pitch)); CHECK(fabs(pitch) < .001);
    pc_input_motion(&s, 100, 0, 220);
    CHECK(pc_input_look(&s, 221, .1f, 0, .5f, &yaw, &pitch)); CHECK(fabs(yaw + 15) < .001);
    pc_input_motion(&s, 100, 0, 300);
    CHECK(!pc_input_look(&s, 450, .1f, 0, 1, &yaw, &pitch));
    pc_input_motion(&s, 100, 0, 500); pc_input_capture(&s, 0);
    CHECK(!pc_input_look(&s, 501, .1f, 0, 1, &yaw, &pitch));
    pc_input_capture(&s, 1); pc_input_key(&s, 1, 1); pc_input_capture(&s, 0);
    CHECK(!pc_input_down(&s, 1, 600));
    pc_input_capture(&s, 1); pc_input_wheel(&s, 2);
    CHECK(pc_input_down(&s, 0x86, 700)); CHECK(!pc_input_down(&s, 0x86, 761));
    CHECK(pc_input_down(&s, 0x86, 802));
    pc_input_focus(&a, 1); pc_input_capture(&a, 1);
    pc_input_focus(&b, 1); pc_input_capture(&b, 1);
    /* Equal counts must yield equal angles whether delivered as one large
       event or many events and whether gameplay runs at 30 or 120 Hz. */
    pc_input_motion(&a, 400, 100, 1000); CHECK(pc_input_look(&a, 1001, .1f, 0, 1, &ya, &pa));
    for (i = 0; i < 100; i++) {
        pc_input_motion(&b, 4, 1, 1000 + (uint64_t)i * 8);
        CHECK(pc_input_look(&b, 1001 + (uint64_t)i * 8, .1f, 0, 1, &yb, &pb));
    }
    CHECK(fabs(ya - yb) < .001 && fabs(pa - pb) < .001);
    pc_input_motion(&a, 1000000, 1000000, 2000);
    CHECK(pc_input_look(&a, 2001, .1f, 0, 1, &ya, &pa));
    CHECK(ya >= -180 && ya <= 180 && pa == -80);
    pc_input_motion(&a, NAN, 0, 2100); CHECK(!a.dx);

    /* Exercise the actual binding parser, Windows host adapter, guest
       address guards and direct camera update together, without OS injection. */
    snprintf(replay_file, sizeof replay_file, "%s\\pc-test-replay.txt", argv[2]);
    f = fopen(replay_file, "w"); CHECK(f);
    fputs("0 key 87 1\n0 key 68 1\n0 key 82 1\n0 key 1 1\n0 key 2 1\n0 key 17 1\n0 wheel 1 0\n0 mouse 100 20\n", f); fclose(f);
    _putenv_s("TSFP_PC_INPUT_REPLAY", replay_file);
    _putenv_s("RECOMP_INPUT_CONFIG", argv[1]);
    replay_epoch=GetTickCount64();
    pc_controls_attach(NULL);
    CHECK(recomp_bindings_sample(0, &pad));
    CHECK(pad.sThumbLY > 23000 && pad.sThumbLX == pad.sThumbLY && hypot(pad.sThumbLX, pad.sThumbLY) <= 32767);
    CHECK(pad.bAnalogButtons[XBOX_BUTTON_X] == 255);
    CHECK(pad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] == 255);
    CHECK(pad.bAnalogButtons[XBOX_BUTTON_LTRIGGER] == 0);
    CHECK(pad.wButtons & XBOX_GAMEPAD_LEFT_THUMB);
    CHECK(pad.wButtons & XBOX_GAMEPAD_RIGHT_THUMB);
    CHECK(pad.wButtons & XBOX_GAMEPAD_DPAD_RIGHT);
    CHECK(recomp_bindings_present_mask() == 1);
    guest = VirtualAlloc(NULL, 0x800000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); CHECK(guest);
    *(float *)(guest + 0x20384) = 55;
    /* A network client's current player can have a nonzero guest player ID,
       even though its keyboard/controller is on local port 1 (index 0). */
    *(uint32_t *)(guest + 0x10000) = 1;
    pc_controls_apply_mouse(0x10000, 0x20000, (ptrdiff_t)guest);
    CHECK(fabs(*(float *)(guest + 0x10100) + 10) < .001);
    /* Player IDs must not act as camera eligibility or host port checks. */
    for (i = 0; i < 16; ++i) {
        float *camera_yaw, *camera_pitch, fov;
        *(uint32_t *)(guest + 0x10000) = (uint32_t)i;
        CHECK(pc_camera_fields(0x10000, 0x20000, (ptrdiff_t)guest,
                               &camera_yaw, &camera_pitch, &fov));
        CHECK(camera_yaw == (float *)(guest + 0x10100));
        CHECK(camera_pitch == (float *)(guest + 0x10104) && fov == 55);
        CHECK(!pc_camera_fields(0, 0x20000, (ptrdiff_t)guest,
                                &camera_yaw, &camera_pitch, &fov));
        CHECK(!pc_camera_fields(0x10000, 0, (ptrdiff_t)guest,
                                &camera_yaw, &camera_pitch, &fov));
        *(float *)(guest + 0x20384) = NAN;
        CHECK(!pc_camera_fields(0x10000, 0x20000, (ptrdiff_t)guest,
                                &camera_yaw, &camera_pitch, &fov));
        *(float *)(guest + 0x20384) = 55;
    }
    CHECK(fabs(*(float *)(guest + 0x10104) + 2) < .001);
    pc_controls_apply_mouse(0x10000, 0x20000, (ptrdiff_t)guest);
    CHECK(fabs(*(float *)(guest + 0x10100) + 10) < .001);
    /* Reproduce issue #7 through the real host adapter: raw mouse counts
       move the active terminal pointer instead of the hidden player view. */
    *(uint32_t *)(guest + 0x10114) = 0x2000;
    *(uint32_t *)(guest + 0x76c27c) = 0x21000;
    *(uint32_t *)(guest + 0x2107c) = 0x22000;
    *(uint32_t *)(guest + 0x22014) = 0x10000;
    *(uint32_t *)(guest + 0x22030) = 0x23000;
    *(uint32_t *)(guest + 0x4e7944) = 1;
    *(float *)(guest + 0x22038) = 100;
    *(float *)(guest + 0x2203c) = 100;
    *(float *)(guest + 0x20384) = 27.5f; /* Pointer ignores scoped FOV. */
    snprintf(state_file, sizeof state_file, "%s\\terminal-state.txt", argv[2]);
    f = fopen(state_file, "w"); CHECK(f); fputs("1 1", f); fclose(f);
    _putenv_s("TSFP_PC_INPUT_STATE", state_file);
    pc_controls_attach(NULL); /* Restart the bounded replay with fresh motion. */
    pc_controls_apply_mouse(0x10000, 0x20000, (ptrdiff_t)guest);
    CHECK(fabs(*(float *)(guest + 0x22038) - 110) < .001);
    CHECK(fabs(*(float *)(guest + 0x2203c) - 102) < .001);
    CHECK(fabs(*(float *)(guest + 0x10100) + 10) < .001);
    CHECK(fabs(*(float *)(guest + 0x10104) + 2) < .001);
    CHECK(recomp_bindings_sample(0, &pad));
    CHECK(pad.bAnalogButtons[XBOX_BUTTON_A] == 0); /* held entry shot is blocked */
    CHECK(pad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] == 0);
    f = fopen(state_file, "w"); CHECK(f); fputs("1 0", f); fclose(f); Sleep(50);
    CHECK(recomp_bindings_sample(0, &pad));
    f = fopen(state_file, "w"); CHECK(f); fputs("1 1", f); fclose(f); Sleep(50);
    CHECK(recomp_bindings_sample(0, &pad));
    CHECK(pad.bAnalogButtons[XBOX_BUTTON_A] == 255);
    CHECK(pad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] == 0);
    *(uint32_t *)(guest + 0x10114) = 2;
    pc_controls_apply_mouse(0x10000, 0x20000, (ptrdiff_t)guest);
    CHECK(fabs(*(float *)(guest + 0x10100) + 10) < .001);
    CHECK(recomp_bindings_sample(0, &pad));
    CHECK(pad.bAnalogButtons[XBOX_BUTTON_A] == 0);
    CHECK(pad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] == 0); /* held selection cannot fire */
    f = fopen(state_file, "w"); CHECK(f); fputs("1 0", f); fclose(f); Sleep(50);
    CHECK(recomp_bindings_sample(0, &pad));
    f = fopen(state_file, "w"); CHECK(f); fputs("1 1", f); fclose(f); Sleep(50);
    CHECK(recomp_bindings_sample(0, &pad));
    CHECK(pad.bAnalogButtons[XBOX_BUTTON_A] == 0);
    CHECK(pad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] == 255);
    /* Team/custom selectors read D-pad bits: WASD aliases are menu-only. */
    for(unsigned j=0;j<4;j++) {
        const unsigned keys[]={'A','D','W','S'},arrows[]={VK_LEFT,VK_RIGHT,VK_UP,VK_DOWN};
        f=fopen(state_file,"w");CHECK(f);fprintf(f,"%u 1",keys[j]);fclose(f);Sleep(50);
        const unsigned masks[]={XBOX_GAMEPAD_DPAD_LEFT,XBOX_GAMEPAD_DPAD_RIGHT,
            XBOX_GAMEPAD_DPAD_UP,XBOX_GAMEPAD_DPAD_DOWN};
        pc_controls_menu_mode(1);CHECK(pc_controls_key_down(arrows[j]));
        CHECK(recomp_bindings_sample(0,&pad) && (pad.wButtons&masks[j]));
        pc_controls_menu_mode(0);CHECK(!pc_controls_key_down(arrows[j]));
        CHECK(pc_controls_key_down(keys[j]));
        f=fopen(state_file,"w");CHECK(f);fprintf(f,"%u 0",keys[j]);fclose(f);Sleep(50);
        pc_controls_menu_mode(1);CHECK(!pc_controls_key_down(arrows[j]));
    }
    f=fopen(state_file,"w");CHECK(f);fputs("1 1",f);fclose(f);Sleep(50);
    /* A menu click must neither fire nor leak across the gameplay transition. */
    pc_controls_menu_mode(1);
    CHECK(recomp_bindings_sample(0, &pad));
    CHECK(pad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] == 0);
    pc_controls_menu_mode(0);
    CHECK(recomp_bindings_sample(0, &pad));
    CHECK(pad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] == 0);
    f = fopen(state_file, "w"); CHECK(f); fputs("1 0", f); fclose(f); Sleep(50);
    CHECK(recomp_bindings_sample(0, &pad));
    f = fopen(state_file, "w"); CHECK(f); fputs("1 1", f); fclose(f); Sleep(50);
    CHECK(recomp_bindings_sample(0, &pad));
    CHECK(pad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] == 255);
    {
        float yaw=*(float *)(guest+0x10100);
        /* Absolute menu coordinates must not turn the camera or consume the
           next gameplay motion sequence with the same third field. */
        f=fopen(state_file,"w");CHECK(f);fputs("1 0 1 50",f);fclose(f);Sleep(50);
        pc_controls_apply_mouse(0x10000,0x20000,(ptrdiff_t)guest);
        CHECK(fabs(*(float *)(guest+0x10100)-yaw)<.001);
        f=fopen(state_file,"w");CHECK(f);fputs("1 0 1 50 0",f);fclose(f);Sleep(50);
        pc_controls_apply_mouse(0x10000,0x20000,(ptrdiff_t)guest);
        CHECK(fabs(*(float *)(guest+0x10100)-yaw)>1);
        yaw=*(float *)(guest+0x10100);Sleep(50);
        pc_controls_apply_mouse(0x10000,0x20000,(ptrdiff_t)guest);
        CHECK(fabs(*(float *)(guest+0x10100)-yaw)<.001); /* same sequence is not replayed */
    }
    {
        /* Native-rate selection must reach the real Windows terminal adapter,
           including paused stale-whole input and return to original mode. */
        char *saved_rate=NULL;size_t saved_size=0;
        CHECK(!_dupenv_s(&saved_rate,&saved_size,"TSFP_NATIVE_FPS"));
        CHECK(!_putenv_s("TSFP_NATIVE_FPS","144"));
        *(uint32_t *)(guest+0x10114)=0x2000;
        *(float *)(guest+0x22038)=100;*(float *)(guest+0x2203c)=100;
        float old_yaw=*(float *)(guest+0x10100);
        for(int step=0;step<5;++step) {
            *(uint32_t *)(guest+0x4e7944)=(step==1 || step==4)?1:0;
            *(float *)(guest+0x4e794c)=(step==1 || step==4)?0:60.f/144;
            if(step==3) CHECK(!_putenv_s("TSFP_NATIVE_FPS","0"));
            f=fopen(state_file,"w");CHECK(f);
            fprintf(f,"1 0 %d 100 20",step+2);fclose(f);Sleep(50);
            pc_controls_apply_mouse(0x10000,0x20000,(ptrdiff_t)guest);
            const float expected[]={110,110,120,120,130};
            CHECK(fabs(*(float *)(guest+0x22038)-expected[step])<.001);
            CHECK(*(float *)(guest+0x10100)==old_yaw);
        }
        /* Repeat through mounted routing, using the real host adapter and
           changing rate selection without restarting input capture. */
        #define MU(a) (*(uint32_t *)(guest+(a)))
        #define MF(a) (*(float *)(guest+(a)))
        MU(0x10114)=0x80;MU(0x10014)=0x21000;MU(0x2107c)=0x22000;
        MU(0x22c14)=0x30000;MU(0x3007c)=0x31000;
        MU(0x31000)=0x30000;MU(0x31004)=0x4147554e;MU(0x31010)=4;MU(0x31068)=0x21000;
        MF(0x20384)=55;MF(0x31034)=-90;MF(0x31038)=90;
        MF(0x3103c)=-179;MF(0x31040)=179;
        float old_pitch=MF(0x10104);
        CHECK(!_putenv_s("TSFP_NATIVE_FPS","144"));
        for(int step=0;step<5;++step) {
            MU(0x4e7944)=(step==1 || step==4)?1:0;
            MF(0x4e794c)=(step==1 || step==4)?0:60.f/144;
            if(step==3)CHECK(!_putenv_s("TSFP_NATIVE_FPS","0"));
            f=fopen(state_file,"w");CHECK(f);
            fprintf(f,"1 0 %d 100 20",step+7);fclose(f);Sleep(50);
            pc_controls_apply_mouse(0x10000,0x20000,(ptrdiff_t)guest);
            const float expected[]={10,10,20,20,30};
            CHECK(fabsf(MF(0x31024)+expected[step])<.001f);
            CHECK(fabsf(MF(0x3101c)-expected[step]*.2f)<.001f);
            CHECK(MF(0x10100)==old_yaw && MF(0x10104)==old_pitch);
        }
        MU(0x10114)=2;
        pc_controls_apply_mouse(0x10000,0x20000,(ptrdiff_t)guest);
        CHECK(MF(0x10100)==old_yaw && MF(0x10104)==old_pitch);
        #undef MU
        #undef MF
        CHECK(!_putenv_s("TSFP_NATIVE_FPS",saved_rate?saved_rate:""));
        free(saved_rate);
    }
    {
        /* Real binding sampler + Windows event/replay adapter: a complete R
         * tap between samples survives, and only old keyboard Fire is held. */
        f=fopen(state_file,"w");CHECK(f);fputs("0 0",f);fclose(f);
        f=fopen(replay_file,"w");CHECK(f);
        unsigned at=(unsigned)(GetTickCount64()-replay_epoch);
        fprintf(f,"0 key 1 1\n%u key 82 1\n%u key 82 0\n%u key 1 0\n%u key 1 1\n",
                at+100,at+105,at+250,at+255);fclose(f);
        *(uint32_t *)(guest+0x10114)=2;
        *(uint32_t *)(guest+0x105a0)=1;
        *(uint32_t *)(guest+0x101c8)=1;
        pc_controls_menu_mode(0);pc_controls_attach(NULL);
        pc_controls_apply_mouse(0x10000,0x20000,(ptrdiff_t)guest);
        Sleep(150);pc_controls_apply_mouse(0x10000,0x20000,(ptrdiff_t)guest);
        CHECK(recomp_bindings_sample(0,&pad));
        CHECK(pad.bAnalogButtons[XBOX_BUTTON_X]==255);
        CHECK(pad.bAnalogButtons[XBOX_BUTTON_RTRIGGER]==0);
        *(uint32_t *)(guest+0x105a0)=7;
        pc_controls_apply_mouse(0x10000,0x20000,(ptrdiff_t)guest);
        Sleep(40);pc_controls_apply_mouse(0x10000,0x20000,(ptrdiff_t)guest);
        CHECK(recomp_bindings_sample(0,&pad));
        CHECK(pad.bAnalogButtons[XBOX_BUTTON_X]==0 && pad.bAnalogButtons[XBOX_BUTTON_RTRIGGER]==0);
        Sleep(100);pc_controls_apply_mouse(0x10000,0x20000,(ptrdiff_t)guest);
        CHECK(recomp_bindings_sample(0,&pad));
        CHECK(pad.bAnalogButtons[XBOX_BUTTON_RTRIGGER]==255);
    }
    VirtualFree(guest, 0, MEM_RELEASE);
    puts("PASS: PC input focus/capture, released buttons, wheel queue, raw-count camera math, pitch limits, FOV scaling, poll independence, WASD/reload/fire bindings and guest camera integration");
    return 0;
}
