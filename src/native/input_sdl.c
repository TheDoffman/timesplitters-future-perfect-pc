/* SDL events are pumped on Cocoa's main thread. Guest input polls may happen
 * on workers: publish key/button state atomically, never call Cocoa there. */
#include "input_sdl.h"
#include "settings_macos.h"
#include "input_bindings.h"
#include "pc_prompts.h"
#include "pc_camera.h"
#include "pc_input.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static atomic_uchar keys[256];
static atomic_int focused;
static PcInput camera_input;
static pthread_mutex_t camera_lock=PTHREAD_MUTEX_INITIALIZER;
static atomic_int mouse_enabled=1, mouse_invert;
static _Atomic float mouse_sensitivity=0.10f;
static int captured;
static int capture_click;

void tsfp_native_mouse_config(int enabled,double sensitivity,int invert_y) {
    atomic_store(&mouse_enabled,!!enabled);
    atomic_store(&mouse_invert,!!invert_y);
    atomic_store(&mouse_sensitivity,isfinite(sensitivity)?fmax(0.001,fmin(10,sensitivity)):0.10);
}

static int virtual_key(SDL_Scancode code) {
    if(code>=SDL_SCANCODE_A && code<=SDL_SCANCODE_Z)return 'A'+code-SDL_SCANCODE_A;
    if(code>=SDL_SCANCODE_1 && code<=SDL_SCANCODE_9)return '1'+code-SDL_SCANCODE_1;
    if(code==SDL_SCANCODE_0)return '0';
    if(code>=SDL_SCANCODE_F1 && code<=SDL_SCANCODE_F12)return 0x70+code-SDL_SCANCODE_F1;
    if(code>=SDL_SCANCODE_F13 && code<=SDL_SCANCODE_F24)return 0x7c+code-SDL_SCANCODE_F13;
    switch(code) {
    case SDL_SCANCODE_RETURN:case SDL_SCANCODE_KP_ENTER:return 0x0d;
    case SDL_SCANCODE_ESCAPE:return 0x1b;
    case SDL_SCANCODE_BACKSPACE:return 8;
    case SDL_SCANCODE_TAB:return 9;
    case SDL_SCANCODE_SPACE:return 32;
    case SDL_SCANCODE_UP:return 0x26;case SDL_SCANCODE_DOWN:return 0x28;
    case SDL_SCANCODE_LEFT:return 0x25;case SDL_SCANCODE_RIGHT:return 0x27;
    case SDL_SCANCODE_PAGEUP:return 0x21;case SDL_SCANCODE_PAGEDOWN:return 0x22;
    case SDL_SCANCODE_HOME:return 0x24;case SDL_SCANCODE_END:return 0x23;
    case SDL_SCANCODE_INSERT:return 0x2d;case SDL_SCANCODE_DELETE:return 0x2e;
    case SDL_SCANCODE_LSHIFT:return 0xa0;case SDL_SCANCODE_RSHIFT:return 0xa1;
    case SDL_SCANCODE_LCTRL:return 0xa2;case SDL_SCANCODE_RCTRL:return 0xa3;
    case SDL_SCANCODE_LALT:return 0xa4;case SDL_SCANCODE_RALT:return 0xa5;
    default:return 0;
    }
}
void pc_controls_get_mouse(double *s,int *enabled,int *invert) {
    *s=atomic_load(&mouse_sensitivity);*enabled=atomic_load(&mouse_enabled);*invert=atomic_load(&mouse_invert);
}
void pc_controls_set_mouse(double s,int enabled,int invert) {tsfp_native_mouse_config(enabled,s,invert);}
void tsfp_native_key_name(int key,char *out,size_t bytes) {
    const char *name=NULL;
    switch(key){case 0:name="Unbound";break;case 1:name="Mouse 1";break;case 2:name="Mouse 2";break;
    case 4:name="Mouse 3";break;case 5:name="Mouse 4";break;case 6:name="Mouse 5";break;
    case 0x86:name="Wheel up";break;case 0x87:name="Wheel down";break;
    case 0x10:name="Shift";break;case 0x11:name="Control";break;case 0x12:name="Option";break;}
    if(!name)for(int sc=1;sc<SDL_SCANCODE_COUNT;sc++)if(virtual_key(sc)==key){name=SDL_GetScancodeName(sc);break;}
    if(name&&*name)snprintf(out,bytes,"%s",name);else snprintf(out,bytes,"Key %d",key);
}
static void clear_mouse(void) {
    pthread_mutex_lock(&camera_lock);camera_input.dx=camera_input.dy=0;pthread_mutex_unlock(&camera_lock);
}
/* Main-thread only: worker polls see only the cleared/published atomics. */
static void set_capture(SDL_Window *window,int wanted) {
    wanted=!!wanted && atomic_load(&focused) && atomic_load(&mouse_enabled);
    if(window && !SDL_SetWindowRelativeMouseMode(window,wanted)) {
        fprintf(stderr,"[native-input] cannot %s mouse capture: %s\n",wanted?"enable":"release",SDL_GetError());
        return;
    }
    captured=wanted;clear_mouse();
    for(int vk=1;vk<=6;vk++)if(vk!=3)atomic_store(&keys[vk],0);
    fprintf(stderr,"[native-input] mouse capture %s\n",captured?"on":"off");
}
static void release_all(void) {
    for(int i=0;i<256;i++)atomic_store(&keys[i],0);
    clear_mouse();capture_click=0;
}
void tsfp_native_settings_release_input(SDL_Window *window) {release_all();set_capture(window,0);}
int tsfp_native_input_focused(void) {return atomic_load(&focused);}
int tsfp_native_key_down(int vk) {
    if(!atomic_load(&focused)||vk<=0||vk>255)return 0;
    if(vk==0x10)return atomic_load(&keys[0xa0])||atomic_load(&keys[0xa1]);
    if(vk==0x11)return atomic_load(&keys[0xa2])||atomic_load(&keys[0xa3]);
    if(vk==0x12)return atomic_load(&keys[0xa4])||atomic_load(&keys[0xa5]);
    return atomic_load(&keys[vk]);
}
void tsfp_native_input_event(const SDL_Event *e,SDL_Window *window) {
    if(e->type==SDL_EVENT_WINDOW_FOCUS_GAINED){release_all();atomic_store(&focused,1);fprintf(stderr,"[native-input] window focused\n");}
    if(e->type==SDL_EVENT_WINDOW_FOCUS_LOST){
        atomic_store(&focused,0);release_all();set_capture(window,0);
        fprintf(stderr,"[native-input] focus lost; controls released\n");
    }
    if(e->type==SDL_EVENT_KEY_DOWN||e->type==SDL_EVENT_KEY_UP){
        int vk=virtual_key(e->key.scancode),down=e->type==SDL_EVENT_KEY_DOWN;
        if(vk)atomic_store(&keys[vk],down);
        if(down && !e->key.repeat)pc_prompts_keyboard();
        if(down && !e->key.repeat && e->key.scancode==SDL_SCANCODE_F2 && window){
            tsfp_native_settings_show(window);return;
        }
        if(down && !e->key.repeat && e->key.scancode==SDL_SCANCODE_F11 && window){
            tsfp_native_settings_release_input(window);
            SDL_SetWindowFullscreen(window,!(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN));return;
        }
        if(down && !e->key.repeat && e->key.scancode==SDL_SCANCODE_F1 && window){
            set_capture(window,!captured);
        }
        if(down && e->key.scancode==SDL_SCANCODE_ESCAPE){
            set_capture(window,0);
        }
    }
    if(e->type==SDL_EVENT_MOUSE_BUTTON_DOWN||e->type==SDL_EVENT_MOUSE_BUTTON_UP){
        int down=e->type==SDL_EVENT_MOUSE_BUTTON_DOWN;
        if(down && atomic_load(&focused))pc_prompts_keyboard();
        if(down && !atomic_load(&focused))return;
        if(e->button.button==SDL_BUTTON_LEFT) {
            if(down && window && !captured && atomic_load(&mouse_enabled)) {
                set_capture(window,1);
                capture_click=1; /* A click to enter mouse-look must not fire. */
                return;
            }
            if(capture_click) {
                if(!down)capture_click=0;
                return;
            }
        }
        int vk=e->button.button==SDL_BUTTON_LEFT?1:e->button.button==SDL_BUTTON_RIGHT?2:
               e->button.button==SDL_BUTTON_MIDDLE?4:e->button.button==SDL_BUTTON_X1?5:e->button.button==SDL_BUTTON_X2?6:0;
        if(vk)atomic_store(&keys[vk],e->type==SDL_EVENT_MOUSE_BUTTON_DOWN);
    }
    if(e->type==SDL_EVENT_MOUSE_WHEEL && captured && atomic_load(&focused)) {
        if(e->wheel.y)pc_prompts_keyboard();
        if(e->wheel.y>0)atomic_store(&keys[0x86],1);
        if(e->wheel.y<0)atomic_store(&keys[0x87],1);
    }
    if(e->type==SDL_EVENT_MOUSE_MOTION && captured && atomic_load(&focused)) {
        if(e->motion.xrel || e->motion.yrel)pc_prompts_keyboard();
        pthread_mutex_lock(&camera_lock);
        camera_input.focused=atomic_load(&focused);camera_input.captured=captured;
        pc_input_motion(&camera_input,e->motion.xrel,e->motion.yrel,SDL_GetTicks());
        pthread_mutex_unlock(&camera_lock);
        static unsigned trace_count;
        if(trace_count<8 && getenv("TSFP_NATIVE_TRACE_RENDER")) {
            fprintf(stderr,"[native-input] captured mouse delta %g,%g\n",e->motion.xrel,e->motion.yrel);trace_count++;
        }
    }
}
/* Mouse deltas reach the camera wrapper; controller stick input stays intact. */
void tsfp_native_mouse_sample(XBOX_GAMEPAD *out) {(void)out;}
void pc_controls_apply_mouse(uint32_t player,uint32_t fov_object,ptrdiff_t offset) {
    float *yaw,*pitch,fov;
    if(!pc_camera_fields(player,fov_object,offset,&yaw,&pitch,&fov))return;
    pthread_mutex_lock(&camera_lock);
    camera_input.focused=atomic_load(&focused)&&atomic_load(&mouse_enabled);
    float old_yaw=*yaw,old_pitch=*pitch;
    int changed=pc_input_look(&camera_input,SDL_GetTicks(),atomic_load(&mouse_sensitivity),
                             atomic_load(&mouse_invert),fov<55?fov/55:1,yaw,pitch);
    pthread_mutex_unlock(&camera_lock);
    static unsigned samples;
    if(changed && samples++<12)fprintf(stderr,"[native-mouse] yaw %.3f -> %.3f pitch %.3f -> %.3f fov %.3f\n",old_yaw,*yaw,old_pitch,*pitch,fov);
}
void tsfp_native_input_frame(SDL_Window *window) {
    atomic_store(&keys[0x86],0);atomic_store(&keys[0x87],0);
    if(captured && !atomic_load(&mouse_enabled))set_capture(window,0);
    /* Optional bounded test input feeds SDL events, then the exact same binding
     * path as physical keys. This is deliberately distinct from XAPI replay. */
    static FILE *test_file;
    static int checked;
    if(!checked){
        checked=1;const char *path=getenv("TSFP_NATIVE_TEST_KEYS");
        if(path && *path){test_file=fopen(path,"r");fprintf(stderr,"[native-input] SDL event test file %s: %s\n",path,test_file?"opened":"failed");}
    }
    if(test_file){
        char line[80];clearerr(test_file);
        for(int n=0;n<32 && fgets(line,sizeof line,test_file);n++){
            line[strcspn(line,"\r\n")]=0;
            if(!line[0])continue;
            float mx,my;int button,down;
            if(sscanf(line,"focus %d",&down)==1 && (down==0 || down==1)) {
                SDL_Event e={0};e.type=down?SDL_EVENT_WINDOW_FOCUS_GAINED:SDL_EVENT_WINDOW_FOCUS_LOST;
                e.window.windowID=SDL_GetWindowID(window);SDL_PushEvent(&e);
                fprintf(stderr,"[native-input] test SDL focus %d\n",down);continue;
            }
            if(sscanf(line,"motion %f %f",&mx,&my)==2 && isfinite(mx) && isfinite(my) && fabsf(mx)<=4096 && fabsf(my)<=4096) {
                SDL_Event e={0};e.type=SDL_EVENT_MOUSE_MOTION;e.motion.windowID=SDL_GetWindowID(window);
                e.motion.xrel=mx;e.motion.yrel=my;SDL_PushEvent(&e);
                fprintf(stderr,"[native-input] test SDL motion %g,%g\n",mx,my);continue;
            }
            if(sscanf(line,"button %d %d",&button,&down)==2 && button>=1 && button<=5 && (down==0 || down==1)) {
                SDL_Event e={0};e.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
                e.button.windowID=SDL_GetWindowID(window);e.button.button=button;e.button.down=down;
                SDL_PushEvent(&e);fprintf(stderr,"[native-input] test SDL button %d %d\n",button,down);continue;
            }
            SDL_Scancode code=SDL_GetScancodeFromName(line+1);
            if((line[0]=='+'||line[0]=='-')&&code!=SDL_SCANCODE_UNKNOWN){
                SDL_Event e={0};e.type=line[0]=='+'?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;
                e.key.windowID=SDL_GetWindowID(window);e.key.scancode=code;e.key.down=line[0]=='+';
                SDL_PushEvent(&e);fprintf(stderr,"[native-input] test SDL key %s\n",line);
            }
        }
    }

}
#include "prompt_selftest.inc"
int tsfp_native_input_test(void) {
    int prompt_ok=native_test_prompts();
    SDL_Event e={0};XBOX_GAMEPAD pad;
    e.type=SDL_EVENT_WINDOW_FOCUS_GAINED;tsfp_native_input_event(&e,NULL);
    e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=SDL_SCANCODE_RETURN;tsfp_native_input_event(&e,NULL);
    recomp_bindings_sample(0,&pad);int ok=prompt_ok && pad.bAnalogButtons[XBOX_BUTTON_A]==255;
    e.type=SDL_EVENT_KEY_UP;tsfp_native_input_event(&e,NULL);
    recomp_bindings_sample(0,&pad);ok &= pad.bAnalogButtons[XBOX_BUTTON_A]==0;
    e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=SDL_SCANCODE_DOWN;tsfp_native_input_event(&e,NULL);
    recomp_bindings_sample(0,&pad);ok &= !!(pad.wButtons&XBOX_GAMEPAD_DPAD_DOWN);
    e.key.scancode=SDL_SCANCODE_W;tsfp_native_input_event(&e,NULL);
    recomp_bindings_sample(0,&pad);ok &= pad.sThumbLY>30000;
    /* Both hosts use pc_input.c: diagonal movement must not run faster. */
    e.key.scancode=SDL_SCANCODE_D;tsfp_native_input_event(&e,NULL);
    recomp_bindings_sample(0,&pad);
    ok &= pad.sThumbLX==pad.sThumbLY && pad.sThumbLX>23000 &&
          hypot(pad.sThumbLX,pad.sThumbLY)<=32767;
    e.type=SDL_EVENT_KEY_UP;tsfp_native_input_event(&e,NULL);
    recomp_bindings_sample(0,&pad);ok &= pad.sThumbLX==0 && pad.sThumbLY>30000;
    e.type=SDL_EVENT_WINDOW_FOCUS_LOST;tsfp_native_input_event(&e,NULL);
    recomp_bindings_sample(0,&pad);ok &= pad.wButtons==0 && pad.sThumbLY==0;
    e.type=SDL_EVENT_WINDOW_FOCUS_GAINED;tsfp_native_input_event(&e,NULL);
    recomp_bindings_sample(0,&pad);ok &= pad.wButtons==0 && pad.sThumbLY==0;
    /* Mouse buttons use shared bindings; deltas obey enabled/sensitivity/invert. */
    e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;tsfp_native_input_event(&e,NULL);
    recomp_bindings_sample(0,&pad);ok &= pad.bAnalogButtons[XBOX_BUTTON_RTRIGGER]==255;
    e.type=SDL_EVENT_MOUSE_BUTTON_UP;tsfp_native_input_event(&e,NULL);
    BYTE guest[2048]={0};ptrdiff_t offset=(ptrdiff_t)guest;
    float *yaw=(float*)(guest+0x100+16),*pitch=yaw+1,*fov=(float*)(guest+0x384+512);*fov=55;
    captured=1;tsfp_native_mouse_config(1,0.1,0);
    e.type=SDL_EVENT_MOUSE_MOTION;e.motion.xrel=2;e.motion.yrel=3;tsfp_native_input_event(&e,NULL);
    tsfp_native_input_frame(NULL);recomp_bindings_sample(0,&pad);
    pc_controls_apply_mouse(16,512,offset);
    ok &= !pad.sThumbRX && !pad.sThumbRY && fabsf(*yaw+.2f)<.001f && fabsf(*pitch+.3f)<.001f;
    tsfp_native_mouse_config(1,0.2,1);tsfp_native_input_event(&e,NULL);
    tsfp_native_input_frame(NULL);recomp_bindings_sample(0,&pad);
    pc_controls_apply_mouse(16,512,offset);
    ok &= fabsf(*yaw+.6f)<.001f && fabsf(*pitch-.3f)<.001f;
    *yaw=*pitch=0;*fov=27.5f;tsfp_native_input_event(&e,NULL);
    pc_controls_apply_mouse(16,512,offset);ok &= fabsf(*yaw+.2f)<.001f && fabsf(*pitch-.3f)<.001f;
    tsfp_native_mouse_config(0,0.1,0);recomp_bindings_sample(0,&pad);
    ok &= pad.sThumbRX==0 && pad.sThumbRY==0;
    captured=0;tsfp_native_mouse_config(1,0.1,0);release_all();
    /* Exercise capture activation through a real SDL window, including the
     * swallowed first click and immediate release of a published look sample. */
    SDL_Window *window=NULL;
    if(SDL_InitSubSystem(SDL_INIT_VIDEO))
        window=SDL_CreateWindow("Native mouse capture regression",320,240,0);
    ok &= window!=NULL;
    if(window) {
        e.type=SDL_EVENT_WINDOW_FOCUS_GAINED;tsfp_native_input_event(&e,window);
        e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;
        tsfp_native_input_event(&e,window);recomp_bindings_sample(0,&pad);
        ok &= captured && SDL_GetWindowRelativeMouseMode(window) && pad.bAnalogButtons[XBOX_BUTTON_RTRIGGER]==0;
        e.type=SDL_EVENT_MOUSE_BUTTON_UP;tsfp_native_input_event(&e,window);
        e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;tsfp_native_input_event(&e,window);
        recomp_bindings_sample(0,&pad);ok &= pad.bAnalogButtons[XBOX_BUTTON_RTRIGGER]==255;
        e.type=SDL_EVENT_MOUSE_MOTION;e.motion.xrel=10;e.motion.yrel=-10;
        tsfp_native_input_event(&e,window);tsfp_native_input_frame(window);
        recomp_bindings_sample(0,&pad);*yaw=*pitch=0;*fov=55;
        pc_controls_apply_mouse(16,512,offset);ok &= *yaw<0 && *pitch>0 && !pad.sThumbRX && !pad.sThumbRY;
        e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=SDL_SCANCODE_ESCAPE;
        tsfp_native_input_event(&e,window);recomp_bindings_sample(0,&pad);
        ok &= !captured && !SDL_GetWindowRelativeMouseMode(window) && !pad.sThumbRX && !pad.sThumbRY && !pad.bAnalogButtons[XBOX_BUTTON_RTRIGGER];
        *yaw=*pitch=0;pc_controls_apply_mouse(16,512,offset);ok &= *yaw==0 && *pitch==0;
        e.type=SDL_EVENT_MOUSE_MOTION;e.motion.xrel=50;e.motion.yrel=50;
        tsfp_native_input_event(&e,window);tsfp_native_input_frame(window);
        recomp_bindings_sample(0,&pad);ok &= !pad.sThumbRX && !pad.sThumbRY;
        e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=SDL_SCANCODE_F1;e.key.repeat=false;
        tsfp_native_input_event(&e,window);ok &= captured;
        tsfp_native_input_event(&e,window);ok &= !captured;
        e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;
        tsfp_native_input_event(&e,window);ok &= captured;
        e.type=SDL_EVENT_WINDOW_FOCUS_LOST;tsfp_native_input_event(&e,window);
        recomp_bindings_sample(0,&pad);ok &= !captured && !pad.sThumbRX && !pad.sThumbRY && !pad.wButtons;
        e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;
        tsfp_native_input_event(&e,window);ok &= !captured;
        e.type=SDL_EVENT_WINDOW_FOCUS_GAINED;tsfp_native_input_event(&e,window);
        tsfp_native_mouse_config(0,0.1,0);
        e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;
        tsfp_native_input_event(&e,window);ok &= !captured;
        tsfp_native_mouse_config(1,0.1,0);tsfp_native_input_event(&e,window);ok &= captured;
        tsfp_native_mouse_config(0,0.1,0);tsfp_native_input_frame(window);
        ok &= !captured && !SDL_GetWindowRelativeMouseMode(window);
        tsfp_native_mouse_config(1,0.1,0);release_all();
        SDL_DestroyWindow(window);
    }
    /* Exercise real SDL mapping/hotplug plus keyboard-player controller sharing. */
    SDL_VirtualJoystickDesc desc;SDL_INIT_INTERFACE(&desc);
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;desc.name="Native input regression pad";
    desc.naxes=SDL_GAMEPAD_AXIS_COUNT;desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    desc.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;
    desc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
    SDL_JoystickID id=SDL_AttachVirtualJoystick(&desc);
    SDL_Joystick *joy=id?SDL_OpenJoystick(id):NULL;
    ok &= joy!=NULL;
    if(joy) {
        SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_SOUTH,true);
        SDL_Delay(300);recomp_bindings_sample(0,&pad);
        ok &= pad.bAnalogButtons[XBOX_BUTTON_A]==255 && !pc_prompts_keyboard_active();
        SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_SOUTH,false);
        SDL_Delay(3);recomp_bindings_sample(0,&pad);
        ok &= pad.bAnalogButtons[XBOX_BUTTON_A]==0;
        SDL_CloseJoystick(joy);SDL_DetachVirtualJoystick(id);
        SDL_Delay(300);recomp_bindings_sample(0,&pad);
        ok &= pad.bAnalogButtons[XBOX_BUTTON_A]==0;
    }
    fprintf(stderr,"[native-input-test] bindings, press/release, movement, focus, mouse settings/click capture/release, SDL pad hotplug: %s\n",ok?"PASS":"FAIL");
    return ok?0:1;
}

#include "settings_selftest.inc"
