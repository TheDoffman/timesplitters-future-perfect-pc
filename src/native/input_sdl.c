#include <signal.h>
#include "crash_posix.h"
/* SDL events are pumped on Cocoa's main thread. Guest input polls may happen
 * on workers: publish key/button state atomically, never call Cocoa there. */
#include "input_sdl.h"
#include "settings_macos.h"
#include "input_bindings.h"
#include "pc_prompts.h"
#include "pc_settings.h"
#include "pc_camera.h"
#include "pc_reload_guest.h"
#include "pc_mounted.h"
#include "pc_terminal.h"
#include "pc_native_rate.h"
#include "pc_debug.h"
#include "pc_input.h"
#include "pc_text_input.h"
#include "pc_menu_mouse.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
/* SDL queues the request safely from a guest worker; Present owns exit. */
int tsfp_native_request_quit(void) {
    SDL_Event event={0};event.type=SDL_EVENT_QUIT;
    return SDL_PushEvent(&event);
}
static atomic_uchar keys[256];
static atomic_int focused;
static PcInput camera_input;
static pthread_mutex_t camera_lock=PTHREAD_MUTEX_INITIALIZER;
static atomic_int mouse_enabled=1, mouse_invert;
static _Atomic float mouse_sensitivity=0.10f;
static atomic_int captured;
static int capture_click;
static atomic_int menu_active, menu_capture_request;
/* Published under camera_lock; only the SDL thread calls window APIs. */
static PcMenuPointer menu_pointer_state;
static unsigned menu_button_block;
static float menu_wheel_remainder;
static Uint64 menu_wheel_at;
static PcTextInput text_input;
int pc_controls_weapon_slots_active(void) {
    pthread_mutex_lock(&camera_lock);
    int active=atomic_load(&focused) && captured && !atomic_load(&menu_active) && !text_input.owner;
    pthread_mutex_unlock(&camera_lock);
    return active;
}

void pc_controls_reload_sample(const unsigned char roles[256],int active,
                               int *reload,int *fire,int *secondary) {
    pthread_mutex_lock(&camera_lock);
    active=active && atomic_load(&focused) && captured && !atomic_load(&menu_active) && !text_input.owner;
    pc_reload_sample(&camera_input.reload,roles,active,SDL_GetTicks(),reload,fire,secondary);
    pthread_mutex_unlock(&camera_lock);
}
/* Publish edges as well as held state: SDL may deliver down/up in one pump. */
static void reload_key_event(unsigned vk,int down) {
    pthread_mutex_lock(&camera_lock);
    if(vk>0 && vk<256 && atomic_load(&focused)) {
        pc_reload_event(&camera_input.reload,vk,camera_input.keys[vk],down,SDL_GetTicks());
        camera_input.keys[vk]=!!down;
        if(vk>=0xa0 && vk<=0xa5) {
            unsigned first=vk&~1u,alias=0x10+(vk-0xa0)/2;
            int held=camera_input.keys[first]||camera_input.keys[first+1];
            pc_reload_event(&camera_input.reload,alias,camera_input.keys[alias],held,SDL_GetTicks());
            camera_input.keys[alias]=held;
        }
    }
    pthread_mutex_unlock(&camera_lock);
}

void pc_controls_text_mode(uint32_t owner) {
    pthread_mutex_lock(&camera_lock);
    if(text_input.owner!=owner) {
        pc_text_session(&text_input,owner);
        for(unsigned i=8;i<256;i++)if(pc_text_key(i))atomic_store(&keys[i],0);
    }
    pthread_mutex_unlock(&camera_lock);
}
unsigned pc_controls_text_event(uint32_t owner) {
    pthread_mutex_lock(&camera_lock);
    unsigned event=text_input.owner==owner?pc_text_get(&text_input):0;
    pthread_mutex_unlock(&camera_lock);
    return event;
}

void pc_controls_menu_mode(int active) {
    pthread_mutex_lock(&camera_lock);
    if(atomic_load(&menu_active)!=!!active) {
        menu_button_block=(menu_pointer_state.left?1u:0u)|(menu_pointer_state.right?2u:0u);
        menu_pointer_state.wheel=0;menu_wheel_remainder=0;
        camera_input.dx=camera_input.dy=0;
        pc_reload_reset(&camera_input.reload);
        atomic_store(&menu_active,!!active);
        atomic_store(&menu_capture_request,active?1:2);
    }
    pthread_mutex_unlock(&camera_lock);
}
static PcMenuPointer menu_pointer_snapshot(int consume) {
    pthread_mutex_lock(&camera_lock);
    PcMenuPointer p=menu_pointer_state;
    p.valid=p.valid && atomic_load(&focused) && atomic_load(&menu_active);
    p.wheel=consume && p.valid && SDL_GetTicks()-menu_wheel_at<250?p.wheel:0;
    if(consume)menu_pointer_state.wheel=0;
    pthread_mutex_unlock(&camera_lock);
    if(pc_settings_capture_blocked())p.valid=p.left=p.right=p.wheel=0;
    return p;
}
PcMenuPointer pc_controls_menu_pointer(void) {return menu_pointer_snapshot(1);}
PcMenuPointer pc_controls_menu_hover_pointer(void) {return menu_pointer_snapshot(0);}

void tsfp_native_mouse_config(int enabled,double sensitivity,int invert_y) {
    atomic_store(&mouse_enabled,!!enabled);
    atomic_store(&mouse_invert,!!invert_y);
    atomic_store(&mouse_sensitivity,isfinite(sensitivity)?fmax(0.001,fmin(10,sensitivity)):0.10);
}

int tsfp_native_virtual_key(SDL_Scancode code) {
    if(code>=SDL_SCANCODE_A && code<=SDL_SCANCODE_Z)return 'A'+code-SDL_SCANCODE_A;
    if(code>=SDL_SCANCODE_1 && code<=SDL_SCANCODE_9)return '1'+code-SDL_SCANCODE_1;
    if(code==SDL_SCANCODE_0)return '0';
    if(code>=SDL_SCANCODE_F1 && code<=SDL_SCANCODE_F12)return 0x70+code-SDL_SCANCODE_F1;
    if(code>=SDL_SCANCODE_F13 && code<=SDL_SCANCODE_F24)return 0x7c+code-SDL_SCANCODE_F13;
    if(code>=SDL_SCANCODE_KP_1 && code<=SDL_SCANCODE_KP_9)return 0x61+code-SDL_SCANCODE_KP_1;
    switch(code) {
    case SDL_SCANCODE_KP_0:return 0x60;
    case SDL_SCANCODE_KP_MULTIPLY:return 0x6a;case SDL_SCANCODE_KP_PLUS:return 0x6b;
    case SDL_SCANCODE_KP_MINUS:return 0x6d;case SDL_SCANCODE_KP_PERIOD:return 0x6e;
    case SDL_SCANCODE_KP_DIVIDE:return 0x6f;case SDL_SCANCODE_NUMLOCKCLEAR:return 0x90;
    case SDL_SCANCODE_CAPSLOCK:return 0x14;case SDL_SCANCODE_PRINTSCREEN:return 0x2c;
    case SDL_SCANCODE_SCROLLLOCK:return 0x91;case SDL_SCANCODE_PAUSE:return 0x13;
    case SDL_SCANCODE_SEMICOLON:return 0xba;case SDL_SCANCODE_EQUALS:return 0xbb;
    case SDL_SCANCODE_COMMA:return 0xbc;case SDL_SCANCODE_MINUS:return 0xbd;
    case SDL_SCANCODE_PERIOD:return 0xbe;case SDL_SCANCODE_SLASH:return 0xbf;
    case SDL_SCANCODE_GRAVE:return 0xc0;case SDL_SCANCODE_LEFTBRACKET:return 0xdb;
    case SDL_SCANCODE_BACKSLASH:return 0xdc;case SDL_SCANCODE_RIGHTBRACKET:return 0xdd;
    case SDL_SCANCODE_APOSTROPHE:return 0xde;case SDL_SCANCODE_NONUSBACKSLASH:return 0xe2;
    case SDL_SCANCODE_LGUI:return 0x5b;case SDL_SCANCODE_RGUI:return 0x5c;
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
    if(!name)for(int sc=1;sc<SDL_SCANCODE_COUNT;sc++)if(tsfp_native_virtual_key(sc)==key){name=SDL_GetScancodeName(sc);break;}
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
    pthread_mutex_lock(&camera_lock);pc_reload_reset(&camera_input.reload);
    memset(camera_input.keys,0,sizeof camera_input.keys);pthread_mutex_unlock(&camera_lock);
    for(int vk=1;vk<=6;vk++)if(vk!=3)atomic_store(&keys[vk],0);
    fprintf(stderr,"[native-input] mouse capture %s\n",captured?"on":"off");
}
static void release_all(void) {
    for(int i=0;i<256;i++)atomic_store(&keys[i],0);
    clear_mouse();capture_click=0;
    pthread_mutex_lock(&camera_lock);
    menu_pointer_state.valid=menu_pointer_state.left=menu_pointer_state.right=menu_pointer_state.wheel=0;
    menu_button_block=0;menu_wheel_remainder=0;
    text_input.head=text_input.count=0;
    pc_reload_reset(&camera_input.reload);memset(camera_input.keys,0,sizeof camera_input.keys);
    pthread_mutex_unlock(&camera_lock);
}
void tsfp_native_settings_release_input(SDL_Window *window) {release_all();set_capture(window,0);}
int tsfp_native_input_focused(void) {return atomic_load(&focused);}
int pc_controls_binding_keys(unsigned char down[256]) {
    memset(down,0,256);
    int focus=atomic_load(&focused);
    if(!focus)return 0;
    for(unsigned i=1;i<256;i++)down[i]=atomic_load(&keys[i]);
    down[0x10]=down[0xa0]||down[0xa1];down[0x11]=down[0xa2]||down[0xa3];down[0x12]=down[0xa4]||down[0xa5];
    memset(down+0xa0,0,6);
    pthread_mutex_lock(&camera_lock);
    if(SDL_GetTicks()-menu_wheel_at<250) {down[0x86]|=menu_pointer_state.wheel>0;down[0x87]|=menu_pointer_state.wheel<0;}
    pthread_mutex_unlock(&camera_lock);return 1;
}
int tsfp_native_key_down(int vk) {
    if(!atomic_load(&focused)||vk<=0||vk>255)return 0;
    pthread_mutex_lock(&camera_lock);
    int typing=text_input.owner && pc_text_key((unsigned)vk);
    pthread_mutex_unlock(&camera_lock);
    if(typing)return 0;
    if(vk==1 || vk==2) {
        pthread_mutex_lock(&camera_lock);
        int blocked=atomic_load(&menu_active) || (menu_button_block&(vk==1?1u:2u));
        pthread_mutex_unlock(&camera_lock);
        if(blocked)return 0;
    }
    if(vk==1) {
        int down=atomic_load(&keys[1]);
        pthread_mutex_lock(&camera_lock);
        int suppress=pc_input_terminal_suppresses_left(&camera_input,down);
        pthread_mutex_unlock(&camera_lock);
        return suppress?0:down;
    }
    if(vk==0x10)return atomic_load(&keys[0xa0])||atomic_load(&keys[0xa1]);
    if(vk==0x11)return atomic_load(&keys[0xa2])||atomic_load(&keys[0xa3]);
    if(vk==0x12)return atomic_load(&keys[0xa4])||atomic_load(&keys[0xa5]);
    return atomic_load(&keys[vk]);
}
void tsfp_native_input_event(const SDL_Event *e,SDL_Window *window) {
    if(e->type==SDL_EVENT_MOUSE_MOTION || e->type==SDL_EVENT_MOUSE_BUTTON_DOWN || e->type==SDL_EVENT_MOUSE_BUTTON_UP) {
        int motion=e->type==SDL_EVENT_MOUSE_MOTION;
        float x=motion?e->motion.x:e->button.x,y=motion?e->motion.y:e->button.y;
        if(motion && atomic_load(&menu_active) && atomic_load(&focused) && (e->motion.xrel || e->motion.yrel))pc_prompts_keyboard();
        pthread_mutex_lock(&camera_lock);
        menu_pointer_state.x=x;menu_pointer_state.y=y;
        menu_pointer_state.valid=atomic_load(&focused) && x>=0 && y>=0 &&
            x<menu_pointer_state.width && y<menu_pointer_state.height;
        pthread_mutex_unlock(&camera_lock);
    }
    if(e->type==SDL_EVENT_WINDOW_MOUSE_LEAVE) {
        pthread_mutex_lock(&camera_lock);menu_pointer_state.valid=0;pthread_mutex_unlock(&camera_lock);
    }
    if(e->type==SDL_EVENT_WINDOW_FOCUS_GAINED){release_all();atomic_store(&focused,1);fprintf(stderr,"[native-input] window focused\n");}
    if(e->type==SDL_EVENT_WINDOW_FOCUS_LOST){
        atomic_store(&focused,0);release_all();set_capture(window,0);
        fprintf(stderr,"[native-input] focus lost; controls released\n");
    }
    if(e->type==SDL_EVENT_TEXT_INPUT) {
        pthread_mutex_lock(&camera_lock);
        if(atomic_load(&focused))pc_text_utf8(&text_input,e->text.text);
        pthread_mutex_unlock(&camera_lock);
        return;
    }
    if(e->type==SDL_EVENT_KEY_DOWN||e->type==SDL_EVENT_KEY_UP){
        int vk=tsfp_native_virtual_key(e->key.scancode),down=e->type==SDL_EVENT_KEY_DOWN;
        if(pc_settings_capture_blocked()) {
            if(vk){atomic_store(&keys[vk],down);reload_key_event(vk,down);}
            if(down && atomic_load(&focused))pc_settings_capture_key(vk);
            return;
        }
        pthread_mutex_lock(&camera_lock);
        int typing=text_input.owner && pc_text_key((unsigned)vk);
        if(typing && down && atomic_load(&focused))
            pc_text_command(&text_input,(unsigned)vk,e->key.repeat,!!(e->key.mod&(SDL_KMOD_CTRL|SDL_KMOD_GUI)));
        pthread_mutex_unlock(&camera_lock);
        if(typing) { atomic_store(&keys[vk],0);if(down)pc_prompts_keyboard();return; }
        if(vk){atomic_store(&keys[vk],down);reload_key_event(vk,down);}
        if(down && !e->key.repeat)pc_prompts_keyboard();
        if(down && !e->key.repeat && e->key.scancode==SDL_SCANCODE_F9){pc_debug_toggle_fps();return;}
        if(down && !e->key.repeat && e->key.scancode==SDL_SCANCODE_F12 && window){
            tsfp_native_settings_release_input(window);tsfp_native_crash_inhibit(1);pc_debug_show(NULL);tsfp_native_crash_inhibit(0);return;
        }
        if(down && !e->key.repeat && e->key.scancode==SDL_SCANCODE_F2 && window){
            tsfp_native_crash_inhibit(1);tsfp_native_settings_show(window);tsfp_native_crash_inhibit(0);return;
        }
        if(down && !e->key.repeat && e->key.scancode==SDL_SCANCODE_F11 && window){
            tsfp_native_settings_release_input(window);
            SDL_SetWindowFullscreen(window,!(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN));return;
        }
        if(down && !e->key.repeat && e->key.scancode==SDL_SCANCODE_F1 && window && !atomic_load(&menu_active)){
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
        pthread_mutex_lock(&camera_lock);
        if(e->button.button==SDL_BUTTON_LEFT){menu_pointer_state.left=down;if(!down)menu_button_block&=~1u;}
        if(e->button.button==SDL_BUTTON_RIGHT){menu_pointer_state.right=down;if(!down)menu_button_block&=~2u;}
        pthread_mutex_unlock(&camera_lock);
        if(e->button.button==SDL_BUTTON_LEFT) {
            if(down && window && !captured && atomic_load(&mouse_enabled) && !atomic_load(&menu_active)) {
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
        if(vk){atomic_store(&keys[vk],down);reload_key_event(vk,down);}
        if(down && atomic_load(&focused))pc_settings_capture_key(vk);
    }
    if(e->type==SDL_EVENT_MOUSE_WHEEL && atomic_load(&menu_active) && atomic_load(&focused)) {
        float delta=e->wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-e->wheel.y:e->wheel.y;
        if(isfinite(delta) && delta) {
            pc_prompts_keyboard();pthread_mutex_lock(&camera_lock);
            if(SDL_GetTicks()-menu_wheel_at>=250){menu_wheel_remainder=0;menu_pointer_state.wheel=0;}
            if((delta>0)!=(menu_wheel_remainder>0))menu_wheel_remainder=0;
            menu_wheel_remainder+=fmaxf(-8,fminf(8,delta));
            int steps=(int)menu_wheel_remainder;menu_wheel_remainder-=steps;
            if(steps) {
                pc_settings_capture_key(steps>0?0x86:0x87);
                if((steps>0)!=(menu_pointer_state.wheel>0))menu_pointer_state.wheel=0;
                menu_pointer_state.wheel+=steps;
                if(menu_pointer_state.wheel>8)menu_pointer_state.wheel=8;
                if(menu_pointer_state.wheel<-8)menu_pointer_state.wheel=-8;
            }
            menu_wheel_at=SDL_GetTicks();pthread_mutex_unlock(&camera_lock);
        }
    }
    if(e->type==SDL_EVENT_MOUSE_WHEEL && captured && !atomic_load(&menu_active) && atomic_load(&focused)) {
        if(e->wheel.y)pc_prompts_keyboard();
        if(e->wheel.y>0){atomic_store(&keys[0x86],1);reload_key_event(0x86,1);}
        if(e->wheel.y<0){atomic_store(&keys[0x87],1);reload_key_event(0x87,1);}
    }
    if(e->type==SDL_EVENT_MOUSE_MOTION && captured && !atomic_load(&menu_active) && atomic_load(&focused)) {
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
int pc_controls_terminal_click(void) {
    pthread_mutex_lock(&camera_lock);
    int down=!atomic_load(&menu_active) && !(menu_button_block&1) && atomic_load(&focused) && atomic_load(&mouse_enabled) &&
             pc_input_terminal_click(&camera_input,atomic_load(&keys[1]));
    pthread_mutex_unlock(&camera_lock);
    return down;
}
void pc_controls_apply_mouse(uint32_t player,uint32_t fov_object,ptrdiff_t offset) {
    float *yaw,*pitch,fov;
    pthread_mutex_lock(&camera_lock);
    camera_input.focused=atomic_load(&focused)&&atomic_load(&mouse_enabled)&&!atomic_load(&menu_active);
    /* No guest callbacks run before this local mapping snapshot expires. */
    PcGuestRegion region=pc_camera_mouse_region(player,fov_object,offset);
    pc_reload_guest_context(&camera_input.reload,player,offset,SDL_GetTicks(),&region);
    int terminal=pc_terminal_mouse_in_region(&camera_input,SDL_GetTicks(),atomic_load(&mouse_sensitivity),player,offset,pc_native_rate()!=0,&region);
    pc_input_terminal_context(&camera_input,terminal>=0,atomic_load(&keys[1]));
    if(terminal>=0 || pc_mounted_mouse_in_region(&camera_input,SDL_GetTicks(),atomic_load(&mouse_sensitivity),
            atomic_load(&mouse_invert),player,fov_object,offset,pc_native_rate()!=0,&region)>=0 || !pc_camera_fields_in_region(player,fov_object,offset,&yaw,&pitch,&fov,&region)) {
        pthread_mutex_unlock(&camera_lock);
        return;
    }
    float old_yaw=*yaw,old_pitch=*pitch;
    int changed=pc_input_look(&camera_input,SDL_GetTicks(),atomic_load(&mouse_sensitivity),
                             atomic_load(&mouse_invert),fov<55?fov/55:1,yaw,pitch);
    pthread_mutex_unlock(&camera_lock);
    static unsigned samples;
    if(changed && samples++<12)fprintf(stderr,"[native-mouse] yaw %.3f -> %.3f pitch %.3f -> %.3f fov %.3f\n",old_yaw,*yaw,old_pitch,*pitch,fov);
}
void tsfp_native_input_frame(SDL_Window *window) {
    tsfp_native_crash_frame();
    if(window)tsfp_native_crash_focus((SDL_GetWindowFlags(window)&(SDL_WINDOW_INPUT_FOCUS|SDL_WINDOW_MINIMIZED))==SDL_WINDOW_INPUT_FOCUS);
    atomic_store(&keys[0x86],0);atomic_store(&keys[0x87],0);
    reload_key_event(0x86,0);reload_key_event(0x87,0);
    pthread_mutex_lock(&camera_lock);
    int typing=text_input.owner && atomic_load(&focused);
    pthread_mutex_unlock(&camera_lock);
    if(window && typing!=SDL_TextInputActive(window)) {
        if(typing)SDL_StartTextInput(window);else SDL_StopTextInput(window);
    }
    int request=atomic_exchange(&menu_capture_request,0);
    if(request)set_capture(window,request==2);
    if(window) {
        int w,h;SDL_GetWindowSize(window,&w,&h);
        pthread_mutex_lock(&camera_lock);
        menu_pointer_state.width=w;menu_pointer_state.height=h;
        if(request==1) {
            SDL_GetMouseState(&menu_pointer_state.x,&menu_pointer_state.y);
            menu_pointer_state.valid=atomic_load(&focused);
        }
        menu_pointer_state.valid &= menu_pointer_state.x>=0 && menu_pointer_state.y>=0 &&
            menu_pointer_state.x<w && menu_pointer_state.y<h;
        pthread_mutex_unlock(&camera_lock);
    }
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
            if(!strcmp(line,"quit")){SDL_Event e={0};e.type=SDL_EVENT_QUIT;SDL_PushEvent(&e);continue;}
            if(!strcmp(line,"crash")){raise(SIGABRT);continue;}
            float mx,my;int button,down;
            if(sscanf(line,"pointer %f %f",&mx,&my)==2 && isfinite(mx) && isfinite(my) && fabsf(mx)<=16384 && fabsf(my)<=16384) {
                PcMenuPointer old=pc_controls_menu_hover_pointer();
                SDL_Event e={0};e.type=SDL_EVENT_MOUSE_MOTION;e.motion.windowID=SDL_GetWindowID(window);
                e.motion.x=mx;e.motion.y=my;e.motion.xrel=mx-old.x;e.motion.yrel=my-old.y;SDL_PushEvent(&e);
                fprintf(stderr,"[native-input] test SDL pointer %g,%g\n",mx,my);continue;
            }
            if(sscanf(line,"wheel %f",&my)==1 && isfinite(my) && fabsf(my)<=8) {
                SDL_Event e={0};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.windowID=SDL_GetWindowID(window);e.wheel.y=my;
                SDL_PushEvent(&e);fprintf(stderr,"[native-input] test SDL wheel %g\n",my);continue;
            }
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
                PcMenuPointer pointer=pc_controls_menu_hover_pointer();
                SDL_Event e={0};e.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
                e.button.windowID=SDL_GetWindowID(window);e.button.button=button;e.button.down=down;
                e.button.x=pointer.x;e.button.y=pointer.y;
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
#include "menu_input_selftest.inc"
int tsfp_native_input_test(void) {
    int prompt_ok=native_test_prompts();
    SDL_Event e={0};XBOX_GAMEPAD pad;
    e.type=SDL_EVENT_WINDOW_FOCUS_GAINED;tsfp_native_input_event(&e,NULL);
    e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=SDL_SCANCODE_RETURN;tsfp_native_input_event(&e,NULL);
    recomp_bindings_sample(0,&pad);int ok=prompt_ok && pad.bAnalogButtons[XBOX_BUTTON_A]==255;
    e.type=SDL_EVENT_KEY_UP;tsfp_native_input_event(&e,NULL);
    recomp_bindings_sample(0,&pad);ok &= pad.bAnalogButtons[XBOX_BUTTON_A]==0;
    PcDebugOptions debug_before,debug_now;pc_debug_get(&debug_before);
    e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=SDL_SCANCODE_F9;tsfp_native_input_event(&e,NULL);
    pc_debug_get(&debug_now);ok &= debug_now.show_fps==!debug_before.show_fps;
    e.key.repeat=true;tsfp_native_input_event(&e,NULL);
    pc_debug_get(&debug_now);ok &= debug_now.show_fps==!debug_before.show_fps;
    e.type=SDL_EVENT_KEY_UP;e.key.repeat=false;tsfp_native_input_event(&e,NULL);
    pc_debug_get(&debug_now);ok &= debug_now.show_fps==!debug_before.show_fps;
    pc_debug_set(&debug_before);
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
    /* Compare batched counts with per-event camera updates, then pitch limits
     * and old motion suppression through the native guest-field adapter. */
    *yaw=*pitch=0;*fov=55;tsfp_native_mouse_config(1,0.1,0);
    e.motion.xrel=400;e.motion.yrel=100;tsfp_native_input_event(&e,NULL);
    pc_controls_apply_mouse(16,512,offset);float batched_yaw=*yaw,batched_pitch=*pitch;
    *yaw=*pitch=0;
    for(int n=0;n<100;n++){e.motion.xrel=4;e.motion.yrel=1;tsfp_native_input_event(&e,NULL);pc_controls_apply_mouse(16,512,offset);}
    ok &= fabsf(*yaw-batched_yaw)<.001f && fabsf(*pitch-batched_pitch)<.001f;
    e.motion.yrel=100000;e.motion.xrel=0;tsfp_native_input_event(&e,NULL);pc_controls_apply_mouse(16,512,offset);ok &= *pitch==-80;
    e.motion.yrel=-100000;tsfp_native_input_event(&e,NULL);pc_controls_apply_mouse(16,512,offset);ok &= *pitch==80;
    e.motion.yrel=10;tsfp_native_input_event(&e,NULL);
    pthread_mutex_lock(&camera_lock);camera_input.motion_at=SDL_GetTicks()-101;pthread_mutex_unlock(&camera_lock);
    *yaw=*pitch=0;pc_controls_apply_mouse(16,512,offset);ok &= *yaw==0 && *pitch==0;
    tsfp_native_mouse_config(0,0.1,0);recomp_bindings_sample(0,&pad);
    ok &= pad.sThumbRX==0 && pad.sThumbRY==0;
    /* Contextual left click must emit only Confirm while the pointer is
     * active, and require release before normal Fire resumes. */
    tsfp_native_mouse_config(1,0.1,0);
    camera_input.focused=camera_input.captured=1;
    pc_input_terminal_context(&camera_input,1,0);
    e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;
    tsfp_native_input_event(&e,NULL);recomp_bindings_sample(0,&pad);
    ok &= pad.bAnalogButtons[XBOX_BUTTON_A]==255 && pad.bAnalogButtons[XBOX_BUTTON_RTRIGGER]==0;
    pc_input_terminal_context(&camera_input,0,1);
    recomp_bindings_sample(0,&pad);
    ok &= pad.bAnalogButtons[XBOX_BUTTON_A]==0 && pad.bAnalogButtons[XBOX_BUTTON_RTRIGGER]==0;
    e.type=SDL_EVENT_MOUSE_BUTTON_UP;tsfp_native_input_event(&e,NULL);recomp_bindings_sample(0,&pad);
    e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;tsfp_native_input_event(&e,NULL);recomp_bindings_sample(0,&pad);
    ok &= pad.bAnalogButtons[XBOX_BUTTON_A]==0 && pad.bAnalogButtons[XBOX_BUTTON_RTRIGGER]==255;
    {
        /* Route fractional terminal time through the actual SDL adapter.
         * This is synthetic SDL input, not physical-device latency evidence. */
        unsigned char *terminal_guest=calloc(1,0x800000);
        const char *rate_env=getenv("TSFP_NATIVE_FPS");
        char *saved_rate=rate_env?strdup(rate_env):NULL;
        ok &= terminal_guest!=NULL && (!rate_env || saved_rate!=NULL);
        if(terminal_guest && (!rate_env || saved_rate)) {
            ptrdiff_t terminal_offset=(ptrdiff_t)terminal_guest;
            #define TU(a) (*(uint32_t *)(terminal_guest+(a)))
            #define TF(a) (*(float *)(terminal_guest+(a)))
            TU(0x10114)=0x2000;TU(0x76c27c)=0x21000;TU(0x2107c)=0x22000;
            TU(0x22014)=0x10000;TU(0x22030)=0x23000;
            TF(0x20384)=55;TF(0x22038)=100;TF(0x2203c)=100;
            ok &= setenv("TSFP_NATIVE_FPS","144",1)==0;
            for(int step=0;step<5;++step) {
                TU(0x4e7944)=(step==1 || step==4)?1:0;
                TF(0x4e794c)=(step==1 || step==4)?0:60.f/144;
                if(step==3)ok &= setenv("TSFP_NATIVE_FPS","0",1)==0;
                e.type=SDL_EVENT_MOUSE_MOTION;e.motion.xrel=100;e.motion.yrel=20;
                tsfp_native_input_event(&e,NULL);
                pc_controls_apply_mouse(0x10000,0x20000,terminal_offset);
                const float expected[]={110,110,120,120,130};
                ok &= fabsf(TF(0x22038)-expected[step])<.001f && TF(0x10100)==0;
            }
            /* The same live-rate selection must reach mounted gun routing. */
            TU(0x10114)=0x80;TU(0x10014)=0x21000;TU(0x2107c)=0x22000;
            TU(0x22c14)=0x30000;TU(0x3007c)=0x31000;
            TU(0x31000)=0x30000;TU(0x31004)=0x4147554e;TU(0x31010)=4;TU(0x31068)=0x21000;
            TF(0x31034)=-90;TF(0x31038)=90;TF(0x3103c)=-179;TF(0x31040)=179;
            ok &= setenv("TSFP_NATIVE_FPS","144",1)==0;
            for(int step=0;step<5;++step) {
                TU(0x4e7944)=(step==1 || step==4)?1:0;
                TF(0x4e794c)=(step==1 || step==4)?0:60.f/144;
                if(step==3)ok &= setenv("TSFP_NATIVE_FPS","0",1)==0;
                e.type=SDL_EVENT_MOUSE_MOTION;e.motion.xrel=100;e.motion.yrel=20;
                tsfp_native_input_event(&e,NULL);
                pc_controls_apply_mouse(0x10000,0x20000,terminal_offset);
                const float expected[]={10,10,20,20,30};
                ok &= fabsf(TF(0x31024)+expected[step])<.001f;
                ok &= fabsf(TF(0x3101c)-expected[step]*.2f)<.001f;
                ok &= TF(0x10100)==0 && TF(0x10104)==0;
            }
            TU(0x10114)=2;
            pc_controls_apply_mouse(0x10000,0x20000,terminal_offset);
            ok &= TF(0x10100)==0 && TF(0x10104)==0;
            if(saved_rate)ok &= setenv("TSFP_NATIVE_FPS",saved_rate,1)==0;
            else ok &= unsetenv("TSFP_NATIVE_FPS")==0;
            #undef TU
            #undef TF
        }
        free(saved_rate);free(terminal_guest);
    }
    /* The synthetic terminal guest was freed above. End its input context
     * before the independent capture/fire test, as a real gameplay frame does. */
    pthread_mutex_lock(&camera_lock);
    pc_input_terminal_context(&camera_input,0,0);
    pthread_mutex_unlock(&camera_lock);
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
        ok &= native_test_menu_input(window);
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
