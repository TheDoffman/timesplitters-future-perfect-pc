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
static volatile LONG opened;
static PcDeveloperState state={.speed=1.,.manual_speed=1.,.slow_speed=.5,.fast_speed=2.};
static int gameplay,story,network;
/* Presentation-thread-owned navigation and settings drafts. */
enum { HOME, SETTINGS, KEYBOARD, DEBUG, LOGS };
enum { VISIBLE=13 };
static int page,selected,scroll,slot,capture_action=-1,edit_number,conflict_key;
static PcSettings draft;
static unsigned char previous[256];
static int previous_left;
static char status[160],number[32];
static char result_message[160];
static ULONGLONG result_until;
static char recent[24][144];
static int recent_count;
static struct {int mode;float position[3],yaw,pitch;} flight;

int pc_developer_open(void) {return InterlockedCompareExchange(&opened,0,0)!=0;}
void pc_developer_toggle(void) {
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
void pc_developer_report(const char *message) {
    AcquireSRWLockExclusive(&dev_lock);snprintf(result_message,sizeof result_message,"%s",message);
    result_until=GetTickCount64()+5000;
    ReleaseSRWLockExclusive(&dev_lock);
    fprintf(stderr,"[DEVELOPER] %s\n",message);
}
void pc_developer_publish(int play,int campaign,int online) {
    AcquireSRWLockExclusive(&dev_lock);
    gameplay=play;story=campaign;network=online;
    if(!play || online) {if(state.freecam)state.hide_hud=0;state.noclip=state.freecam=0;state.speed=state.manual_speed=1.;state.god=state.infinite_ammo=0;}
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
static int row_count(void) {return page==HOME?14:page==SETTINGS?5:page==KEYBOARD?PC_ACTION_COUNT+6:page==LOGS?recent_count+1:PC_DEBUG_OPTION_COUNT+5;}
static void navigate(int next) {
    fprintf(stderr,"[DEVELOPER] page %d\n",next);
    page=next;selected=scroll=0;capture_action=-1;edit_number=0;status[0]=0;
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
static void activate(int direction) {
    fprintf(stderr,"[DEVELOPER] action page=%d row=%d direction=%d\n",page,selected,direction);
    PcDeveloperState s;pc_developer_get(&s);
    if(page==HOME) {
        if(selected==0){navigate(SETTINGS);return;}
        if(selected==7){pc_developer_close();return;}
        if(selected==8){speed_adjust(1);return;}
        AcquireSRWLockExclusive(&dev_lock);
        switch(selected) {
        case 1:state.hide_hud=!state.hide_hud;break;
        case 2:if(gameplay&&!network){state.noclip=!state.noclip;if(state.freecam)state.hide_hud=0;state.freecam=0;}break;
        case 3:if(gameplay&&!network){state.freecam=!state.freecam;state.noclip=0;state.hide_hud=state.freecam;}break;
        case 4:state.hide_watermark=!state.hide_watermark;break;
        case 5:if(story&&gameplay&&!network)state.checkpoint++;break;
        case 6:break;
        case 9:state.speed_keys=!state.speed_keys;break;
        case 10:case 11:break;
        case 12:if(gameplay&&!network)state.god=!state.god;break;
        case 13:if(gameplay&&!network)state.infinite_ammo=!state.infinite_ammo;break;
        }
        ReleaseSRWLockExclusive(&dev_lock);
        if(selected==6) {
            if(direction)speed_adjust(s.manual_speed+direction*.1);
            else {edit_number=1;number[0]=0;snprintf(status,sizeof status,"Type speed 0.05 to 4.0, then Enter; Escape cancels.");}
        }
        if(selected==10||selected==11) {
            if(direction) {
                AcquireSRWLockExclusive(&dev_lock);
                double *target=selected==10?&state.slow_speed:&state.fast_speed;
                *target=fmax(.05,fmin(4.,*target+direction*.1));
                ReleaseSRWLockExclusive(&dev_lock);
            } else {edit_number=selected==10?3:4;number[0]=0;snprintf(status,sizeof status,"Type held speed 0.05 to 4.0, then Enter.");}
        }
    } else if(page==SETTINGS) {
        switch(selected) {
        case 0:navigate(KEYBOARD);break;
        case 1:navigate(DEBUG);break;
        case 2:tsfp_texture_toggle();break;
        case 3:pc_controls_developer_fullscreen();break;
        default:navigate(HOME);break;
        }
    } else if(page==KEYBOARD) {
        if(selected<PC_ACTION_COUNT) {
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
static void label(int row,char *out,size_t size) {
    PcDeveloperState s;pc_developer_get(&s);
    if(page==HOME) {
        AcquireSRWLockShared(&dev_lock);
        int available=gameplay&&!network,campaign=story&&available;
        ReleaseSRWLockShared(&dev_lock);
        const char *availability=available?"":" (unavailable here)";
        switch(row) {
        case 0:snprintf(out,size,"PC Settings  >");break;
        case 1:snprintf(out,size,"HUD and UI: %s",s.hide_hud?"Hidden":"Shown");break;
        case 2:snprintf(out,size,"No-clip: %s%s",s.noclip?"On":"Off",availability);break;
        case 3:snprintf(out,size,"Free-cam: %s%s",s.freecam?"On":"Off",availability);break;
        case 4:snprintf(out,size,"Version watermark: %s",s.hide_watermark?"Hidden":"Shown");break;
        case 5:snprintf(out,size,"Advance to next checkpoint%s",campaign?"":" (Story only, offline)");break;
        case 6:snprintf(out,size,"Manual game speed: %.2fx   Left/Right or Enter to type",s.manual_speed);break;
        case 7:snprintf(out,size,"Return to game (F1)");break;
        case 8:snprintf(out,size,"Reset game speed to 1.00x");break;
        case 9:snprintf(out,size,"Speed keys outside menu: %s",s.speed_keys?"On":"Off");break;
        case 10:snprintf(out,size,"Hold [: %.2fx   Enter to configure",s.slow_speed);break;
        case 11:snprintf(out,size,"Hold ]: %.2fx   Enter to configure",s.fast_speed);break;
        case 12:snprintf(out,size,"God mode: %s%s",s.god?"On":"Off",availability);break;
        default:snprintf(out,size,"Infinite ammo: %s%s",s.infinite_ammo?"On":"Off",availability);break;
        }
    } else if(page==SETTINGS) {
        static const char *names[]={"Keyboard & Mouse  >","Debug  >","Enhanced Textures","Fullscreen","Back"};
        snprintf(out,size,"%s%s",names[row],row==2?(tsfp_texture_is_4k()?": On":": Off"):"");
    } else if(page==KEYBOARD) {
        if(row<PC_ACTION_COUNT) {
            char a[48],b[48];pc_settings_key_name(draft.keys[row][0],a,sizeof a);pc_settings_key_name(draft.keys[row][1],b,sizeof b);
            snprintf(out,size,"%s: %s%s%s / %s%s%s",pc_action_labels[row],slot?"":"[",a,slot?"":"]",slot?"[":"",b,slot?"]":"");
        } else {
            switch(row-PC_ACTION_COUNT) {
            case 0:snprintf(out,size,"Mouse sensitivity: %.4g",draft.sensitivity);break;
            case 1:snprintf(out,size,"Invert Y: %s",draft.invert_y?"On":"Off");break;
            case 2:snprintf(out,size,"Keyboard and mouse: %s",draft.enabled?"Enabled":"Disabled");break;
            case 3:snprintf(out,size,"Restore defaults (save to apply)");break;
            case 4:snprintf(out,size,"Save and apply");break;
            default:snprintf(out,size,"Back (discard unsaved edits)");break;
            }
        }
    } else if(page==LOGS) {
        snprintf(out,size,"%s",row==recent_count?"Back / refresh by reopening":recent[row]);
    } else {
        PcDebugOptions options;pc_debug_get(&options);
        if(row<PC_DEBUG_OPTION_COUNT)snprintf(out,size,"%s: %s",pc_debug_option(row)->label,pc_debug_option_value(&options,row)?"On":"Off");
        else switch(row-PC_DEBUG_OPTION_COUNT) {
        case 0:{PcDebugSnapshot d;pc_debug_snapshot(&d);snprintf(out,size,"%.1f FPS / %.2f ms   Errors %lu  Warnings %lu  Slow frames %lu",d.fps,d.frame_ms,d.errors,d.warnings,d.slow_frames);break;}
        case 1:snprintf(out,size,"Recent diagnostics  >");break;
        case 2:snprintf(out,size,"Open logs folder");break;
        case 3:snprintf(out,size,"Clear recent diagnostics");break;
        default:snprintf(out,size,"Back");break;
        }
    }
}
/* Shared geometry is consumed by both hit testing and backend rendering. */
static float layout(float w,float h,float *x,float *y) {
    float scale=fminf(w/840.f,h/620.f);
    *x=(w-780.f*scale)*.5f;*y=(h-560.f*scale)*.5f;
    return scale;
}
void pc_developer_frame(void) {
    static int announced;
    if(!announced){announced=1;fprintf(stderr,"[DEVELOPER] overlay loop ready\n");}
    unsigned char keys[256]={0};int focused=pc_controls_developer_keys(keys);
    /* Deterministic input replays publish raw virtual keys, while real host
     * adapters consume F1 before the guest. Toggle after releasing input locks. */
    if(focused && keys[0x70]&&!previous[0x70])pc_developer_toggle();
    int open=pc_developer_open();
    if(!open) {capture_action=-1;edit_number=0;}
    AcquireSRWLockExclusive(&dev_lock);
    state.speed=state.manual_speed;
    if(focused && gameplay && !network && (open||state.speed_keys) && capture_action<0 && !edit_number) {
        if(keys[0xdd])state.speed=state.fast_speed;
        else if(keys[0xdb])state.speed=state.slow_speed;
    }
    ReleaseSRWLockExclusive(&dev_lock);
    if(open&&focused) {
        PcMenuPointer p=pc_controls_developer_pointer();
        int left=keys[0x25]&&!previous[0x25],right=keys[0x27]&&!previous[0x27];
        int enter=keys[13]&&!previous[13],escape=keys[27]&&!previous[27];
        if(capture_action>=0) {
            if(escape){capture_action=-1;status[0]=0;}
            else if(conflict_key && enter) {
                pc_settings_assign(&draft,capture_action,slot,conflict_key,1);capture_action=-1;status[0]=0;
            } else if(!conflict_key)for(int k=1;k<256;k++)if(keys[k]&&!previous[k]) {
                int key=k==0x2e?0:k;
                int result=pc_settings_assign(&draft,capture_action,slot,key,0);
                if(result<0){conflict_key=key;snprintf(status,sizeof status,"Already assigned. Enter moves this binding; Escape cancels.");}
                else if(result){capture_action=-1;status[0]=0;}
                else snprintf(status,sizeof status,"That key is reserved. Choose another key.");
                break;
            }
        } else if(edit_number) {
            if(escape){edit_number=0;status[0]=0;}
            else if(enter) {
                char *end;double v=strtod(number,&end);
                if(*number&&!*end&&isfinite(v)&&v>=(edit_number==2?.001:.05)&&v<=(edit_number==2?10.:4.)) {
                    if(edit_number==1)speed_adjust(v);
                    else if(edit_number==2)draft.sensitivity=v;
                    else {AcquireSRWLockExclusive(&dev_lock);if(edit_number==3)state.slow_speed=v;else state.fast_speed=v;ReleaseSRWLockExclusive(&dev_lock);}
                    edit_number=0;status[0]=0;
                } else snprintf(status,sizeof status,"Enter a value in the displayed range.");
            } else {
                size_t n=strlen(number);
                if(keys[8]&&!previous[8]&&n)number[--n]=0;
                for(int k=0;k<10;k++)if((keys['0'+k]&&!previous['0'+k])||(keys[0x60+k]&&!previous[0x60+k]))if(n<sizeof number-1){number[n++]=(char)('0'+k);number[n]=0;}
                if(((keys[0xbe]&&!previous[0xbe])||(keys[0x6e]&&!previous[0x6e]))&&n<sizeof number-1){number[n++]='.';number[n]=0;}
            }
        } else {
            if(escape){if(page==HOME)pc_developer_close();else navigate(page==SETTINGS?HOME:page==LOGS?DEBUG:SETTINGS);}
            if(keys[0x26]&&!previous[0x26])selected=(selected+row_count()-1)%row_count();
            if(keys[0x28]&&!previous[0x28])selected=(selected+1)%row_count();
            if(keys[0x21]&&!previous[0x21])selected=selected>VISIBLE?selected-VISIBLE:0;
            if(keys[0x22]&&!previous[0x22])selected=selected+VISIBLE<row_count()?selected+VISIBLE:row_count()-1;
            if(p.wheel)selected=(int)fmax(0,fmin(row_count()-1,selected-p.wheel));
            if(selected<scroll)scroll=selected;if(selected>=scroll+VISIBLE)scroll=selected-VISIBLE+1;
            if(p.valid) {
                float x,y,scale=layout(p.width,p.height,&x,&y);
                int row=(int)floorf((p.y-y-64*scale)/(30*scale));
                if(p.x>=x&&p.x<x+780*scale&&row>=0&&row<VISIBLE&&row+scroll<row_count()&&p.left&&!previous_left) {selected=row+scroll;enter=1;}
            }
            if(enter||left||right)activate(right?1:left?-1:0);
        }
        previous_left=p.left;
    }
    memcpy(previous,keys,sizeof previous);
}
unsigned pc_developer_lines(unsigned width,unsigned height,PcDeveloperLine *out,unsigned capacity) {
    if(!pc_developer_open()||capacity<3||!width||!height)return 0;
    float x,y,s=layout((float)width,(float)height,&x,&y);unsigned n=0;
    PcDeveloperLine *line=&out[n++];
    *line=(PcDeveloperLine){{0},x,y,780*s,32*s};
    snprintf(line->text,sizeof line->text,"Developer Tools%s   |   F1 closes",page==HOME?"":page==SETTINGS?" / PC Settings":page==KEYBOARD?" / Keyboard & Mouse":" / Debug");
    for(int r=scroll;r<row_count()&&r<scroll+VISIBLE&&n+2<capacity;r++) {
        char text[144];label(r,text,sizeof text);line=&out[n++];
        *line=(PcDeveloperLine){{0},x,y+(64+(r-scroll)*30)*s,780*s,28*s};
        snprintf(line->text,sizeof line->text,"%s %s",r==selected?">":" ",text);
    }
    line=&out[n++];*line=(PcDeveloperLine){{0},x,y+480*s,780*s,28*s};
    AcquireSRWLockShared(&dev_lock);
    snprintf(line->text,sizeof line->text,"%s%s",edit_number?"Value: ":"",edit_number?number:status[0]?status:result_message[0]&&GetTickCount64()<result_until?result_message:"Select: arrows / mouse   Use: Enter / click   Back: Esc   Scroll: PgUp/PgDn");
    ReleaseSRWLockShared(&dev_lock);
    line=&out[n++];*line=(PcDeveloperLine){{0},x,y+514*s,780*s,28*s};
    snprintf(line->text,sizeof line->text,"%s",capture_action>=0||edit_number?status:page==KEYBOARD?"Left/Right: binding slot   Enter: rebind   Save to apply":"Flight: configured move/look, Interact/Crouch: up/down, Shift: boost");
    return n;
}
