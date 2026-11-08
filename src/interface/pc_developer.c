#include "pc_developer.h"
#include "pc_controls.h"
#include "pc_text_input.h"
#include "pc_settings.h"
#include "pc_debug.h"
#include "texture_mode.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SRWLOCK dev_lock=SRWLOCK_INIT;
static volatile LONG opened,shortcut_blocked;
static char lobby_notice[1152];
static ULONGLONG lobby_notice_at;
static void (*audio_set_speed)(float);
/* Retained for a possible return to the named lobby disclaimer. */
static int lobby_disclaimer_enabled;
static PcDeveloperState state={.speed=1.,.manual_speed=1.,.slow_speed=.5,.fast_speed=2.,.audio_speed=1};
static int gameplay,story,network,movie_active,match_tools_allowed;
/* Presentation-thread-owned navigation and settings drafts. */
enum { HOME, SETTINGS, KEYBOARD, DEBUG, LOGS };
static int page,selected,slot,capture_action=-1,edit_number,conflict_key;
static PcSettings draft;
static unsigned char previous[256];
static int previous_left;
static char status[160],number[32];
static char result_message[160];
static ULONGLONG result_until;
static char recent[24][144];
static int recent_count;
static struct {int mode;float position[3],yaw,pitch;} flight;

static PcVideoSnapshot video_snapshot;
static PcVideoRequest video_request;
static unsigned video_serial;
static void (*video_guest_tick)(void);
void pc_developer_video_register(void (*tick)(void)) {video_guest_tick=tick;}
void pc_developer_video_guest_tick(void) {if(video_guest_tick)video_guest_tick();}
void pc_developer_video_publish(const PcVideoSnapshot *snapshot) {
    AcquireSRWLockExclusive(&dev_lock);video_snapshot=*snapshot;ReleaseSRWLockExclusive(&dev_lock);
}
void pc_developer_video_snapshot(PcVideoSnapshot *snapshot) {
    AcquireSRWLockShared(&dev_lock);*snapshot=video_snapshot;ReleaseSRWLockShared(&dev_lock);
}
int pc_developer_video_take(PcVideoRequest *request) {
    AcquireSRWLockExclusive(&dev_lock);*request=video_request;video_request.serial=0;ReleaseSRWLockExclusive(&dev_lock);return request->serial!=0;
}
unsigned pc_developer_video_request(int action,const PcVideoValues *desired) {
    AcquireSRWLockExclusive(&dev_lock);
    if(video_request.serial){ReleaseSRWLockExclusive(&dev_lock);return 0;}
    video_request.action=action;if(desired)video_request.desired=*desired;
    video_request.serial=++video_serial;if(!video_request.serial)video_request.serial=++video_serial;
    unsigned serial=video_request.serial;ReleaseSRWLockExclusive(&dev_lock);return serial;
}
int pc_developer_enabled(void) {return pc_settings_developer_enabled();}
int pc_developer_tools_allowed(void) {
    AcquireSRWLockShared(&dev_lock);int allowed=!network||match_tools_allowed;ReleaseSRWLockShared(&dev_lock);return allowed;
}
void pc_developer_match_policy(int online,int allowed) {
    AcquireSRWLockExclusive(&dev_lock);network=!!online;match_tools_allowed=!!allowed;
    if(online&&!allowed) {
        if(state.freecam)state.hide_hud=0;
        state.noclip=state.freecam=0;
        state.god=state.infinite_ammo=0;state.speed=1.;
    }
    ReleaseSRWLockExclusive(&dev_lock);
}
int pc_developer_shortcut_key(int key) {
    if(key<=0 || key>255 || !pc_developer_enabled() || pc_settings_capture_blocked() ||
       InterlockedCompareExchange(&shortcut_blocked,0,0))return 0;
    int keys[PC_BINDING_SLOTS];pc_settings_developer_keys(keys);
    for(unsigned i=0;i<PC_BINDING_SLOTS;i++)if(keys[i]==key)return 1;
    return 0;
}
void pc_developer_shortcut_name(char *out,size_t bytes) {
    int keys[PC_BINDING_SLOTS];pc_settings_developer_keys(keys);out[0]=0;
    for(unsigned i=0;i<PC_BINDING_SLOTS;i++)if(keys[i]) {
        char key[48];pc_settings_key_name(keys[i],key,sizeof key);
        size_t n=strlen(out);snprintf(out+n,bytes-n,"%s%s",n?" / ":"",key);
    }
    if(!out[0])snprintf(out,bytes,"the shortcut (currently unbound)");
}
int pc_developer_set_enabled(int enabled) {
    char error[160];
    if(!pc_settings_developer_save(enabled,error,sizeof error)){pc_developer_report(error);return 0;}
    if(!enabled) {
        pc_developer_close();
        AcquireSRWLockExclusive(&dev_lock);
        state.hide_hud=state.hide_watermark=state.noclip=state.freecam=state.god=state.infinite_ammo=0;
        state.speed=state.manual_speed=1.;ReleaseSRWLockExclusive(&dev_lock);
    }
    return 1;
}
void pc_developer_lobby_notice(const char *names) {
    AcquireSRWLockExclusive(&dev_lock);
    snprintf(lobby_notice,sizeof lobby_notice,"%s",names?names:"");lobby_notice_at=GetTickCount64();
    ReleaseSRWLockExclusive(&dev_lock);
}
int pc_developer_open(void) {return InterlockedCompareExchange(&opened,0,0)!=0;}
void pc_developer_toggle(void) {
    if(!pc_developer_enabled())return;
    LONG old=InterlockedCompareExchange(&opened,0,0);
    while(InterlockedCompareExchange(&opened,!old,old)!=old)old=InterlockedCompareExchange(&opened,0,0);
    if(!old){pc_settings_capture_end();pc_controls_text_mode(0);}else pc_controls_developer_release();
    pc_controls_menu_mode(pc_developer_open());
    fprintf(stderr,"[DEVELOPER] overlay %s\n",pc_developer_open()?"open":"closed");
}
void pc_developer_close(void) {if(InterlockedExchange(&opened,0))pc_controls_developer_release();pc_controls_menu_mode(0);}
void pc_developer_get(PcDeveloperState *out) {
    AcquireSRWLockShared(&dev_lock);*out=state;ReleaseSRWLockShared(&dev_lock);
}
double pc_developer_speed(void) {
    int enabled=pc_developer_enabled();double speed;
    AcquireSRWLockShared(&dev_lock);speed=enabled&&!network&&(gameplay||movie_active)?state.speed:1.;ReleaseSRWLockShared(&dev_lock);
    return speed;
}
double pc_developer_audio_speed(void) {
    int enabled=pc_developer_enabled();double speed;
    AcquireSRWLockShared(&dev_lock);
    speed=enabled&&state.audio_speed&&!network&&(gameplay||movie_active)?state.speed:1.;
    ReleaseSRWLockShared(&dev_lock);return speed;
}
void pc_developer_movie(int active) {
    AcquireSRWLockExclusive(&dev_lock);movie_active=!!active;
    state.speed=network?1.:state.manual_speed;ReleaseSRWLockExclusive(&dev_lock);
}
void pc_developer_report(const char *message) {
    AcquireSRWLockExclusive(&dev_lock);snprintf(result_message,sizeof result_message,"%s",message);
    result_until=GetTickCount64()+5000;
    ReleaseSRWLockExclusive(&dev_lock);
    fprintf(stderr,"[DEVELOPER] %s\n",message);
}
void pc_developer_publish(int play,int campaign,int online) {
    int enabled=pc_developer_enabled();
    AcquireSRWLockExclusive(&dev_lock);
    if(online&&!network)match_tools_allowed=0;
    gameplay=play;story=campaign;network=online;
    if(!play || (online&&!match_tools_allowed) || !enabled) {if(state.freecam)state.hide_hud=0;state.noclip=state.freecam=0;}
    if(online||!enabled){state.speed=1.;state.god=state.infinite_ammo=0;}
    else if(!play)state.speed=movie_active?state.manual_speed:1.;
    ReleaseSRWLockExclusive(&dev_lock);
}
int pc_developer_hud_visible(void) {PcDeveloperState s;pc_developer_get(&s);return !s.hide_hud;}
int pc_developer_watermark_visible(void) {PcDeveloperState s;pc_developer_get(&s);return !s.hide_watermark;}
int pc_developer_flying(void) {PcDeveloperState s;pc_developer_get(&s);return s.noclip||s.freecam;}
int pc_developer_blocks_player(void) {PcDeveloperState s;pc_developer_get(&s);return s.freecam;}
int pc_developer_mouse(PcInput *input,uint64_t now,float sensitivity,int invert,float scale) {
    AcquireSRWLockExclusive(&dev_lock);
    int active=state.noclip||state.freecam;
    if(active) {
        if(flight.mode&&!pc_developer_open())pc_input_look(input,now,sensitivity,invert,scale,&flight.yaw,&flight.pitch);
        else input->dx=input->dy=0;
    }
    ReleaseSRWLockExclusive(&dev_lock);return active;
}
int pc_developer_flight_step(int mode,const float origin[3],const float direction[3],double seconds,
                             float position[3],float forward[3]) {
    unsigned char keys[256]={0};int focus=pc_controls_developer_keys(keys);
    for(unsigned i=0;i<3;i++)if(!isfinite(origin[i])||!isfinite(direction[i]))return 0;
    PcSettings controls;char error[160];
    if(!pc_settings_read(&controls,error,sizeof error))pc_settings_defaults(&controls);
    int actions[PC_ACTION_COUNT]={0};
    for(unsigned i=0;i<PC_ACTION_COUNT;i++)for(unsigned j=0;j<PC_BINDING_SLOTS;j++) {
        int key=controls.keys[i][j];if(key>0&&key<256)actions[i]|=keys[key];
    }
    AcquireSRWLockExclusive(&dev_lock);
    if(!mode)flight.mode=0;
    else {
        if(flight.mode!=mode) {
            float length=sqrtf(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]);
            if(!(length>.01f)) {ReleaseSRWLockExclusive(&dev_lock);return 0;}
            memcpy(flight.position,origin,sizeof flight.position);
            flight.yaw=atan2f(direction[0],direction[2])*57.295779513f;
            flight.pitch=asinf(fmaxf(-1,fminf(1,direction[1]/length)))*57.295779513f;
            flight.mode=mode;
        }
        float y=flight.yaw*.01745329252f,p=flight.pitch*.01745329252f;
        forward[0]=sinf(y)*cosf(p);forward[1]=sinf(p);forward[2]=cosf(y)*cosf(p);
        if(focus&&!pc_developer_open()&&isfinite(seconds)&&seconds>0) {
            float f=(float)(actions[14]-actions[15]),r=(float)(actions[17]-actions[16]),u=(float)(actions[0]-actions[18]);
            float delta[3]={f*forward[0]-r*cosf(y),f*forward[1]+u,f*forward[2]+r*sinf(y)};
            float length=sqrtf(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]);
            float distance=(float)fmin(seconds,.05)*8.f*(keys[0x10]?4.f:1.f)/fmaxf(1,length);
            for(unsigned i=0;i<3;i++)flight.position[i]+=delta[i]*distance;
        }
        memcpy(position,flight.position,sizeof flight.position);
    }
    ReleaseSRWLockExclusive(&dev_lock);return mode!=0;
}
static void navigate(int next) {
    fprintf(stderr,"[DEVELOPER] page %d\n",next);
    page=next;selected=0;capture_action=-1;edit_number=0;status[0]=0;
    if(page==KEYBOARD && !pc_settings_read(&draft,status,sizeof status))page=SETTINGS;
    if(page==LOGS) {
        PcDebugSnapshot snapshot;pc_debug_snapshot(&snapshot);recent_count=0;
        const char *p=snapshot.recent;
        while(*p) {
            const char *end=strchr(p,'\n');size_t length=end?(size_t)(end-p):strlen(p);
            if(recent_count==24){memmove(recent,recent+1,23*sizeof recent[0]);recent_count--;}
            snprintf(recent[recent_count++],sizeof recent[0],"%.*s",(int)length,p);
            if(!end)break;p=end+1;
        }
    }
}
static void speed_adjust(double value) {
    if(!isfinite(value))return;
    AcquireSRWLockExclusive(&dev_lock);
    if(!network)state.speed=state.manual_speed=fmax(.05,fmin(4.,value));
    ReleaseSRWLockExclusive(&dev_lock);
}
static void flight_toggle(int mode) {
    AcquireSRWLockExclusive(&dev_lock);
    if(gameplay&&(!network||match_tools_allowed)) {
        if(mode==1) {state.noclip=!state.noclip;if(state.freecam)state.hide_hud=0;state.freecam=0;}
        else {state.freecam=!state.freecam;state.noclip=0;state.hide_hud=state.freecam;}
    }
    ReleaseSRWLockExclusive(&dev_lock);
}
void pc_developer_audio_register(void (*set_speed)(float)) {audio_set_speed=set_speed;}
static int action_pressed(int action,const unsigned char keys[256]) {
    int bindings[PC_BINDING_SLOTS];pc_settings_action_keys(action,bindings);
    for(unsigned i=0;i<PC_BINDING_SLOTS;i++) {
        int key=bindings[i];if(key>0&&key<256&&keys[key]&&!previous[key])return 1;
    }
    return 0;
}
static void activate(int direction) {
    fprintf(stderr,"[DEVELOPER] action page=%d row=%d direction=%d\n",page,selected,direction);
    PcDeveloperState s;pc_developer_get(&s);
    if(page==HOME&&(!pc_developer_tools_allowed()||(network&&((selected>=5&&selected<=10)||selected==11||selected==12||selected==13))))return;
    if(page==HOME) {
        if(selected==0){navigate(SETTINGS);return;}
        if(selected==2||selected==3){flight_toggle(selected-1);return;}
        if(selected==7){speed_adjust(1);return;}
        AcquireSRWLockExclusive(&dev_lock);
        switch(selected) {
        case 5:if(story&&gameplay&&!network)state.checkpoint++;break;
        case 6:break;
        case 8:state.speed_keys=!state.speed_keys;break;
        case 9:case 10:break;
        case 11:if(gameplay&&!network)state.god=!state.god;break;
        case 12:if(gameplay&&!network)state.infinite_ammo=!state.infinite_ammo;break;
        case 13:state.audio_speed=!state.audio_speed;break;
        }
        ReleaseSRWLockExclusive(&dev_lock);
        if(selected==6) {
            if(direction)speed_adjust(s.manual_speed+direction*.1);
            else {edit_number=1;number[0]=0;snprintf(status,sizeof status,"Type speed 0.05 to 4.0, then Enter; Escape cancels.");}
        }
        if(selected==9||selected==10) {
            if(direction) {
                AcquireSRWLockExclusive(&dev_lock);
                double *target=selected==9?&state.slow_speed:&state.fast_speed;
                *target=fmax(.05,fmin(4.,*target+direction*.1));
                ReleaseSRWLockExclusive(&dev_lock);
            } else {edit_number=selected==9?3:4;number[0]=0;snprintf(status,sizeof status,"Type held speed 0.05 to 4.0, then Enter.");}
        }
    } else if(page==SETTINGS) {
        switch(selected) {
        case 0:navigate(KEYBOARD);break;
        case 1:navigate(DEBUG);break;
        case 2:tsfp_texture_toggle();break;
        case 3:pc_controls_developer_fullscreen();break;
        case 4:AcquireSRWLockExclusive(&dev_lock);state.hide_hud=!state.hide_hud;ReleaseSRWLockExclusive(&dev_lock);break;
        case 5:AcquireSRWLockExclusive(&dev_lock);state.hide_watermark=!state.hide_watermark;ReleaseSRWLockExclusive(&dev_lock);break;
        default:navigate(HOME);break;
        }
    } else if(page==KEYBOARD) {
        if(selected<PC_ACTION_COUNT) {
            if(!pc_settings_action_editable(draft.preset,selected)) {
                snprintf(status,sizeof status,"Choose Custom in Controls to edit preset gameplay bindings.");return;
            }
            if(direction){slot=!slot;return;}
            capture_action=selected;conflict_key=0;
            snprintf(status,sizeof status,"Press a binding; Delete unbinds, Escape cancels. Editing %s slot.",slot?"alternate":"primary");
        } else switch(selected-PC_ACTION_COUNT) {
        case 0:if(direction)draft.sensitivity=fmax(.001,fmin(10.,draft.sensitivity+direction*.01));else{edit_number=2;number[0]=0;snprintf(status,sizeof status,"Type sensitivity 0.001 to 10, then Enter.");}break;
        case 1:draft.invert_y=!draft.invert_y;break;
        case 2:draft.enabled=!draft.enabled;break;
        case 3:pc_settings_defaults(&draft);break;
        case 4:
            if(pc_settings_save(&draft,status,sizeof status)) {
                pc_controls_set_mouse(draft.sensitivity,draft.enabled,draft.invert_y);
                snprintf(status,sizeof status,"Keyboard and mouse settings saved.");
            }
            break;
        default:navigate(SETTINGS);break;
        }
    } else if(page==LOGS) {
        if(selected==recent_count)navigate(DEBUG);
    } else {
        if(selected<PC_DEBUG_OPTION_COUNT) {
            PcDebugOptions options;pc_debug_get(&options);
            pc_debug_option_set(&options,selected,!pc_debug_option_value(&options,selected));
            if(pc_debug_config_save(&options,status,sizeof status))pc_debug_set(&options);
        } else switch(selected-PC_DEBUG_OPTION_COUNT) {
        case 0:break;
        case 1:navigate(LOGS);break;
        case 2:pc_controls_developer_debug_action(1);break;
        case 3:pc_controls_developer_debug_action(0);break;
        default:navigate(SETTINGS);break;
        }
    }
}
#include "pc_developer_sidebar.inc"
