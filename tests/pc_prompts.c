#include "pc_prompts.h"
#include "pc_controls.h"
#include "texture_mode.h"
#include "input_bindings.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static uint32_t g_esp;
static ptrdiff_t g_xbox_mem_offset;
#include "../src/pc_prompt_guest.inc"
static unsigned fallback_calls;
static void guest_fallback(void) { ++fallback_calls; g_esp+=4; }
static unsigned seen_arg;
static char seen_text[2048];
static void guest_original(void)
{
    uint32_t *frame=(uint32_t *)(g_xbox_mem_offset+g_esp);
    snprintf(seen_text,sizeof seen_text,"%s",(char *)(g_xbox_mem_offset+frame[seen_arg]));
    g_esp+=4;
}
static LRESULT CALLBACK window_proc(HWND h,UINT m,WPARAM w,LPARAM l)
{
    LRESULT result;
    if(pc_controls_message(h,m,w,l,&result)) return result;
    return DefWindowProc(h,m,w,l);
}
static int virtual_pad_test(void)
{
    SDL_VirtualJoystickDesc desc={0};
    SDL_JoystickID id;
    SDL_Joystick *joystick;
    XBOX_GAMEPAD output;
    WNDCLASSA wc={0};
    HWND window;
    char text[100];
    int pads=0;
    CHECK(SDL_Init(SDL_INIT_GAMEPAD));
    SDL_JoystickID *existing=SDL_GetGamepads(&pads);
    SDL_free(existing);
    if(pads) { puts("SKIP: virtual-pad routing test requires no physical controllers attached"); return 0; }
    SDL_INIT_INTERFACE(&desc);
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes=SDL_GAMEPAD_AXIS_COUNT; desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    desc.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;
    desc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
    desc.name="TSFP prompt test controller";
    id=SDL_AttachVirtualJoystick(&desc); CHECK(id);
    joystick=SDL_OpenJoystick(id); CHECK(joystick);
    CHECK(SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,-32768));
    CHECK(SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,-32768));
    wc.lpfnWndProc=window_proc; wc.hInstance=GetModuleHandle(NULL); wc.lpszClassName="TSFP_Prompt_Test";
    CHECK(RegisterClassA(&wc));
    window=CreateWindowA(wc.lpszClassName,"Input prompt test",WS_OVERLAPPEDWINDOW,0,0,320,200,NULL,NULL,wc.hInstance,NULL);
    CHECK(window); pc_controls_attach(window);
    {
        HMENU menu=GetSubMenu(GetMenu(window),0);
        int before=tsfp_texture_is_4k();
        SendMessage(window,WM_SYSKEYDOWN,VK_F10,0);
        CHECK(tsfp_texture_is_4k()!=before);
        CHECK((GetMenuState(menu,PC_TEXTURE_MODE_COMMAND,MF_BYCOMMAND)&MF_CHECKED)==
              (tsfp_texture_is_4k()?MF_CHECKED:0));
        SendMessage(window,WM_SYSKEYDOWN,VK_F10,(LPARAM)1<<30);
        CHECK(tsfp_texture_is_4k()!=before);
        SendMessage(window,WM_SYSKEYUP,VK_F10,0);
        pc_settings_toggle_menu(window); CHECK(!GetMenu(window));
        SendMessage(window,WM_KEYDOWN,VK_F10,0);
        CHECK(tsfp_texture_is_4k()==before);
        pc_settings_toggle_menu(window);
        SendMessage(window,WM_INITMENUPOPUP,(WPARAM)menu,0);
        CHECK((GetMenuState(menu,PC_TEXTURE_MODE_COMMAND,MF_BYCOMMAND)&MF_CHECKED)==
              (before?MF_CHECKED:0));
        SendMessage(window,WM_COMMAND,PC_TEXTURE_MODE_COMMAND,0);
        CHECK(tsfp_texture_is_4k()!=before);
        SendMessage(window,WM_COMMAND,PC_TEXTURE_MODE_COMMAND,0);
        CHECK(tsfp_texture_is_4k()==before);
    }
    ShowWindow(window,SW_SHOW); SetForegroundWindow(window);
    {
        ULONGLONG deadline=GetTickCount64()+1000;
        while(GetForegroundWindow()!=window && GetTickCount64()<deadline) {
            MSG message;
            while(PeekMessage(&message,NULL,0,0,PM_REMOVE)) {
                TranslateMessage(&message);DispatchMessage(&message);
            }
            SetForegroundWindow(window);Sleep(10);
        }
        if(GetForegroundWindow()!=window) {
            SDL_CloseJoystick(joystick);
            CHECK(SDL_DetachVirtualJoystick(id));
            DestroyWindow(window);
            puts("SKIP: virtual-pad routing requires Windows foreground focus; foreground activation was refused");
            return 0;
        }
    }
    CHECK(pc_controls_accept_pad());
    Sleep(260);
    CHECK(recomp_bindings_sample(0,&output));
    CHECK(pc_prompts_keyboard_active());
    CHECK(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_SOUTH,true));
    Sleep(3); CHECK(recomp_bindings_sample(0,&output));
    CHECK(output.bAnalogButtons[XBOX_BUTTON_A]==255 && !pc_prompts_keyboard_active());
    CHECK(!pc_prompts_rewrite("\v2 Select",text,sizeof text));
    SendMessage(window,WM_KEYDOWN,'E',0);
    CHECK(pc_prompts_keyboard_active());
    CHECK(recomp_bindings_sample(0,&output) && pc_prompts_keyboard_active());
    CHECK(pc_prompts_rewrite("\v2 Select",text,sizeof text));
    CHECK(!strcmp(text,"[E] Select"));
    SendMessage(window,WM_KEYUP,'E',0);
    CHECK(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_SOUTH,false));
    CHECK(SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767));
    Sleep(3); CHECK(recomp_bindings_sample(0,&output));
    CHECK(output.bAnalogButtons[XBOX_BUTTON_RTRIGGER]==255 && !pc_prompts_keyboard_active());
    CHECK(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_START,true));
    CHECK(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_DPAD_DOWN,true));
    CHECK(SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_LEFTX,32767));
    CHECK(SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_RIGHTY,32767));
    Sleep(3); CHECK(recomp_bindings_sample(0,&output));
    CHECK((output.wButtons&(XBOX_GAMEPAD_START|XBOX_GAMEPAD_DPAD_DOWN))==
          (XBOX_GAMEPAD_START|XBOX_GAMEPAD_DPAD_DOWN));
    CHECK(output.sThumbLX==32767 && output.sThumbRY==-32767);
    SendMessage(window,WM_LBUTTONDOWN,MK_LBUTTON,0);
    CHECK(pc_prompts_keyboard_active());
    SendMessage(window,WM_LBUTTONUP,0,0);
    ShowWindow(window,SW_HIDE);
    CHECK(!pc_controls_accept_pad());
    CHECK(recomp_bindings_sample(0,&output));
    {
        XBOX_GAMEPAD neutral={0};
        CHECK(!memcmp(&output,&neutral,sizeof output));
    }
    CHECK(SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,-32768));
    SDL_CloseJoystick(joystick);
    CHECK(SDL_DetachVirtualJoystick(id));
    Sleep(260); CHECK(recomp_bindings_sample(0,&output));
    CHECK(pc_prompts_keyboard_active());
    DestroyWindow(window);
    puts("PASS: virtual SDL controller and keyboard share player 1; live prompts switch both ways; held pad does not steal keyboard prompts");
    return 0;
}
int main(int argc,char **argv)
{
    RecompPadState pad={0};
    PcSettings settings;
    int keys[PC_ACTION_COUNT];
    char text[512];
    CHECK(argc==2 || argc==3);
    _putenv_s("RECOMP_INPUT_CONFIG",argv[1]);
    _putenv_s("RECOMP_PAD_API","sdl");
    _putenv_s("TSFP_PC_INPUT_REPLAY","");
    if(argc==3) {
        CHECK(!strcmp(argv[2],"controller-profile"));
        pc_prompts_reset();
        return virtual_pad_test();
    }
    pc_settings_defaults(&settings);
    for(int i=0;i<PC_ACTION_COUNT;i++) keys[i]=settings.keys[i][0];
    pc_prompts_reset();
    CHECK(pc_prompts_keyboard_active());
    CHECK(pc_prompts_rewrite("XBOX LIVE / SYSTEM LINK",text,sizeof text));
    CHECK(!strcmp(text,"ONLINE / SYSTEM LINK"));
    CHECK(pc_prompts_rewrite("Play with friends on Xbox Live",text,sizeof text));
    CHECK(!strcmp(text,"Play with friends on ONLINE"));
    CHECK(!pc_prompts_rewrite("XBOX LIVE",text,6));
    pad.lx=2000; pad.ry=-1000; pad.left_trigger=15;
    pc_prompts_controller(&pad,1); CHECK(pc_prompts_keyboard_active());
    pad.buttons=RECOMP_PAD_A;
    pc_prompts_controller(&pad,1); CHECK(!pc_prompts_keyboard_active());
    CHECK(pc_prompts_rewrite("Xbox Live / SYSTEM LINK",text,sizeof text));
    CHECK(!strcmp(text,"ONLINE / SYSTEM LINK"));
    CHECK(pc_prompts_rewrite("\v2 Xbox Live",text,sizeof text));
    CHECK(!strcmp(text,"\v2 ONLINE"));
    pc_prompts_keyboard();
    pc_prompts_controller(&pad,1); CHECK(pc_prompts_keyboard_active()); /* held button */
    pad.buttons=0;
    pc_prompts_controller(&pad,1); CHECK(pc_prompts_keyboard_active()); /* release */
    pad.buttons=RECOMP_PAD_B;
    pc_prompts_controller(&pad,1); CHECK(!pc_prompts_keyboard_active());
    pc_prompts_keyboard(); pad.lx=16000;
    pc_prompts_controller(&pad,1); CHECK(!pc_prompts_keyboard_active());
    pc_prompts_keyboard(); pad.lx=16500;
    pc_prompts_controller(&pad,1); CHECK(pc_prompts_keyboard_active()); /* stick jitter */
    pad.lx=20000;
    pc_prompts_controller(&pad,1); CHECK(!pc_prompts_keyboard_active());
    pc_prompts_keyboard(); pad.right_trigger=70;
    pc_prompts_controller(&pad,1); CHECK(!pc_prompts_keyboard_active());
    pc_prompts_controller(NULL,0); CHECK(pc_prompts_keyboard_active());
    CHECK(pc_prompts_format("\v2 Select  \v1 Back",text,sizeof text,keys,1));
    CHECK(!strcmp(text,"[E] Select  [F] Back"));
    keys[0]='J'; keys[1]=VK_BACK;
    CHECK(pc_prompts_format("\v2 Select  \v1 Back",text,sizeof text,keys,1));
    CHECK(!strcmp(text,"[J] Select  [Backspace] Back"));
    CHECK(pc_prompts_format("\vG / \vI / \vM / \vE",text,sizeof text,keys,1));
    CHECK(!strcmp(text,"[G] / [Mouse 1] / [Mouse 2] / [Mouse]"));
    CHECK(pc_prompts_format("\v3 \v4 \v5 \v6 \v7",text,sizeof text,keys,1));
    CHECK(!strcmp(text,"[Y] [R] [X] [Z] [Esc]"));
    CHECK(pc_prompts_format("\vD",text,sizeof text,keys,1));
    CHECK(!strcmp(text,"[W/S/A/D]"));
    keys[0]=0;
    CHECK(pc_prompts_format("\v2",text,sizeof text,keys,1));
    CHECK(!strcmp(text,"[Unbound]"));
    CHECK(pc_prompts_format("\vh100\v2\n\v8",text,sizeof text,keys,1));
    CHECK(!strcmp(text,"\vh100[Unbound]\n\v8"));
    CHECK(!pc_prompts_format("ordinary text\v",text,sizeof text,keys,1));
    CHECK(!pc_prompts_format("\v2",text,4,keys,1));
    {
        unsigned char *memory=VirtualAlloc(NULL,0x500000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        CHECK(memory);
        g_xbox_mem_offset=(ptrdiff_t)memory;
        strcpy((char *)memory+0x1000,"\v2 Select  \v1 Back");
        for(unsigned arg=3;arg<=4;arg++) {
            uint32_t *frame=(uint32_t *)(memory+0x10000);
            g_esp=0x10000; seen_arg=arg;
            for(unsigned i=0;i<7;i++) frame[i]=0x12345678;
            frame[arg]=0x1000;
            pc_prompt_guest(guest_original,arg,arg==3 ? 4 : 6);
            CHECK(g_esp==0x10004 && frame[arg]==0x1000 && frame[0]==0x12345678);
            CHECK(!strcmp(seen_text,"[E] Select  [F] Back"));
            CHECK(!strcmp((char *)memory+0x1000,"\v2 Select  \v1 Back"));
        }
        pad.buttons=RECOMP_PAD_A; pc_prompts_controller(&pad,1);
        g_esp=0x10000;
        pc_prompt_guest(guest_original,4,6);
        CHECK(!strcmp(seen_text,"\v2 Select  \v1 Back"));
        CHECK(g_esp==0x10004);
        strcpy((char *)memory+0x1000,"XBOX LIVE / SYSTEM LINK");
        for(unsigned arg=3;arg<=4;arg++) {
            uint32_t *frame=(uint32_t *)(memory+0x10000);
            g_esp=0x10000; seen_arg=arg;
            frame[arg]=0x1000;
            pc_prompt_guest(guest_original,arg,arg==3 ? 4 : 6);
            CHECK(!strcmp(seen_text,"ONLINE / SYSTEM LINK"));
            CHECK(g_esp==0x10004 && frame[arg]==0x1000);
            CHECK(!strcmp((char *)memory+0x1000,"XBOX LIVE / SYSTEM LINK"));
        }
        pc_prompts_reset();
        {
            /* Match stack-local fragments, boundaries and changed protections.
             * Read queries now live in pc_prompts.c; their reduction and nested
             * scope expiry are measured by text_read_scope.c. This fixture
             * checks actual OS protection changes and the scratch/call ABI. */
            uint32_t *frame=(uint32_t *)(memory+0x10000);
            DWORD old;
            seen_arg=3; frame[3]=0x10080;
            strcpy((char *)memory+0x10080,"ordinary text");
            g_esp=0x10000;
            pc_prompt_guest(guest_original,3,4);
            CHECK(!strcmp(seen_text,"ordinary text"));
            strcpy((char *)memory+0x10080,"\v2 Select");
            g_esp=0x10000;
            pc_prompt_guest(guest_original,3,4);
            CHECK(!strcmp(seen_text,"[E] Select"));
            CHECK(frame[3]==0x10080 && g_esp==0x10004);
            strcpy((char *)memory+0x1000,"below stack");
            frame[3]=0x1000; g_esp=0x10000;
            pc_prompt_guest(guest_original,3,4);
            CHECK(!strcmp(seen_text,"below stack"));
            strcpy((char *)memory+0x11ffc,"abcdef");
            CHECK(VirtualProtect(memory+0x12000,0x1000,PAGE_READONLY,&old));
            frame[3]=0x11ffc; g_esp=0x10000;
            pc_prompt_guest(guest_original,3,4);
            CHECK(!strcmp(seen_text,"abcdef"));
            CHECK(VirtualProtect(memory+0x12000,0x1000,PAGE_NOACCESS,&old));
            g_esp=0x10000; fallback_calls=0;
            pc_prompt_guest(guest_fallback,3,4);
            CHECK(fallback_calls==1 && g_esp==0x10004);
            g_esp=0x11ff8;
            pc_prompt_guest(guest_fallback,3,4);
            CHECK(fallback_calls==2 && g_esp==0x11ffc);
            CHECK(VirtualProtect(memory+0x12000,0x1000,PAGE_READWRITE,&old));
            g_esp=0x10000;
            pc_prompt_guest(guest_original,3,4);
            CHECK(!strcmp(seen_text,"abcdef"));
            /* Scratch below ESP can occupy a different protection region.
             * A readable stack must never authorize that write. */
            frame[3]=0x10080;
            strcpy((char *)memory+0x10080,"\v2 Select");
            CHECK(VirtualProtect(memory+0xf000,0x1000,PAGE_READONLY,&old));
            g_esp=0x10000; fallback_calls=0;
            pc_prompt_guest(guest_fallback,3,4);
            CHECK(fallback_calls==1 && g_esp==0x10004);
            CHECK(VirtualProtect(memory+0xf000,0x1000,PAGE_READWRITE,&old));
            g_esp=0x10000;
            pc_prompt_guest(guest_original,3,4);
            CHECK(!strcmp(seen_text,"[E] Select"));
            /* A new call sees changed permissions even at the same address. */
            CHECK(VirtualProtect(memory,0x500000,PAGE_READONLY,&old));
            g_esp=0x10000; fallback_calls=0;
            pc_prompt_guest(guest_fallback,3,4);
            CHECK(fallback_calls==1 && g_esp==0x10004);
            CHECK(VirtualProtect(memory,0x500000,PAGE_READWRITE,&old));
            g_esp=0x10000;
            pc_prompt_guest(guest_original,3,4);
            CHECK(!strcmp(seen_text,"[E] Select"));
            /* Text in a separately protected mapping still gets checked. */
            frame[3]=0x1000;
            strcpy((char *)memory+0x1000,"\v2 Select");
            CHECK(VirtualProtect(memory+0x1000,0x1000,PAGE_READONLY,&old));
            g_esp=0x10000;
            pc_prompt_guest(guest_original,3,4);
            CHECK(!strcmp(seen_text,"[E] Select"));
            CHECK(VirtualProtect(memory+0x1000,0x1000,PAGE_NOACCESS,&old));
            g_esp=0x10000; fallback_calls=0;
            pc_prompt_guest(guest_fallback,3,4);
            CHECK(fallback_calls==1);
            CHECK(VirtualProtect(memory+0x1000,0x1000,PAGE_READWRITE,&old));
            /* Nearby text on either side of non-page-aligned ESP is readable;
             * scratch writes still require current write permission. */
            frame=(uint32_t *)(memory+0x10800);
            frame[3]=0x10780;
            strcpy((char *)memory+0x10780,"\v2 Select");
            g_esp=0x10800;
            pc_prompt_guest(guest_original,3,4);
            CHECK(!strcmp(seen_text,"[E] Select"));
            CHECK(VirtualProtect(memory+0x10000,0x1000,PAGE_READONLY,&old));
            g_esp=0x10800;fallback_calls=0;
            pc_prompt_guest(guest_fallback,3,4);
            CHECK(fallback_calls==1);
            CHECK(VirtualProtect(memory+0x10000,0x1000,PAGE_READWRITE|PAGE_GUARD,&old));
            g_esp=0x10800;fallback_calls=0;
            pc_prompt_guest(guest_fallback,3,4);
            CHECK(fallback_calls==1);
            CHECK(VirtualProtect(memory+0x10000,0x1000,PAGE_READWRITE,&old));
            g_esp=0x10800;
            pc_prompt_guest(guest_original,3,4);
            CHECK(!strcmp(seen_text,"[E] Select"));
            CHECK(!pc_guest_memory(UINTPTR_MAX-4,16,0));
            CHECK(g_esp==0x10804 && frame[3]==0x10780);
            frame[3]=0x10900;
            strcpy((char *)memory+0x10900,"\v2 Select");
            g_esp=0x10800;
            pc_prompt_guest(guest_original,3,4);
            CHECK(!strcmp(seen_text,"[E] Select"));
        }
        VirtualFree(memory,0,MEM_RELEASE);
    }
    CHECK(!virtual_pad_test());
    puts("PASS: input switching, drift/held-button suppression, disconnect, assigned labels, mouse/sticks, trigger labels, unbound keys, formatting, and bounds");
    return 0;
}
