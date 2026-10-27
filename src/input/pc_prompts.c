#include "pc_prompts.h"
#include "pc_controls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_text_read_scope.h"
#include "pc_storage.inc"

#ifdef _MSC_VER
static __declspec(thread) PcTextReadScope text_read_scope;
#else
static _Thread_local PcTextReadScope text_read_scope;
#endif
void pc_prompts_layout_begin(void) { pc_text_read_begin(&text_read_scope); }
void pc_prompts_layout_end(void) { pc_text_read_end(&text_read_scope); }
size_t pc_prompts_read_region(uintptr_t address)
{ return pc_text_read_region(&text_read_scope,address); }
size_t pc_prompts_read_span(uintptr_t address,size_t minimum)
{ return pc_text_read_span(&text_read_scope,address,minimum); }

static SRWLOCK prompt_lock = SRWLOCK_INIT;
static RecompPadState previous_pad;
static int keyboard_active = 1;

static void select_device(int keyboard)
{
    if (keyboard_active == keyboard) return;
    keyboard_active = keyboard;
    fprintf(stderr,"[PC-PROMPTS] %s controls\n",keyboard ? "keyboard/mouse" : "controller");
}

void pc_prompts_reset(void)
{
    AcquireSRWLockExclusive(&prompt_lock);
    memset(&previous_pad,0,sizeof previous_pad);
    keyboard_active=1;
    ReleaseSRWLockExclusive(&prompt_lock);
}

void pc_prompts_keyboard(void)
{
    AcquireSRWLockExclusive(&prompt_lock);
    select_device(1);
    ReleaseSRWLockExclusive(&prompt_lock);
}

static int axis_activity(short current, short *previous, int deadzone)
{
    int value=abs((int)current)>deadzone ? current : 0;
    if (!value) { *previous=0; return 0; }
    if (!*previous || abs(value-(int)*previous)>=2048) {
        *previous=(short)value;
        return 1;
    }
    return 0;
}

void pc_prompts_controller(const RecompPadState *pad, int connected)
{
    int active=0;
    AcquireSRWLockExclusive(&prompt_lock);
    if (!connected) {
        memset(&previous_pad,0,sizeof previous_pad);
        select_device(1);
    } else {
        active=(pad->buttons & ~previous_pad.buttons)!=0;
        active |= pad->left_trigger>=30 &&
            (previous_pad.left_trigger<30 || pad->left_trigger>previous_pad.left_trigger+8);
        active |= pad->right_trigger>=30 &&
            (previous_pad.right_trigger<30 || pad->right_trigger>previous_pad.right_trigger+8);
        active |= axis_activity(pad->lx,&previous_pad.lx,7849);
        active |= axis_activity(pad->ly,&previous_pad.ly,7849);
        active |= axis_activity(pad->rx,&previous_pad.rx,8689);
        active |= axis_activity(pad->ry,&previous_pad.ry,8689);
        previous_pad.buttons=pad->buttons;
        previous_pad.left_trigger=pad->left_trigger;
        previous_pad.right_trigger=pad->right_trigger;
        if (active) select_device(0);
    }
    ReleaseSRWLockExclusive(&prompt_lock);
}

int pc_prompts_keyboard_active(void)
{
    int value;
    AcquireSRWLockShared(&prompt_lock);
    value=keyboard_active;
    ReleaseSRWLockShared(&prompt_lock);
    return value;
}

static void key_label(int key, char *out, size_t capacity)
{
    const char *name=NULL;
    switch(key) {
    case 0: name="Unbound"; break;
    case VK_RETURN: name="Enter"; break;
    case VK_ESCAPE: name="Esc"; break;
    case VK_SPACE: name="Space"; break;
    case VK_BACK: name="Backspace"; break;
    case VK_CONTROL: name="Ctrl"; break;
    case VK_SHIFT: name="Shift"; break;
    case VK_MENU: name="Alt"; break;
    case VK_UP: name="Up"; break;
    case VK_DOWN: name="Down"; break;
    case VK_LEFT: name="Left"; break;
    case VK_RIGHT: name="Right"; break;
    }
    if (name) snprintf(out,capacity,"%s",name);
    else pc_settings_key_name(key,out,capacity);
    /* Key names must not introduce the game's text-formatting controls. */
    for (size_t i=0;out[i];i++) if ((unsigned char)out[i]<32) out[i]='?';
}

static int prompt_actions(unsigned char token, int actions[4], int mouse)
{
    static const int buttons[]={1,0,3,2,4,5,6}; /* B A Y X Black White Start */
    if (token>='1' && token<='7') { actions[0]=buttons[token-'1']; return 1; }
    switch(token) {
    case 'A': actions[0]=12; actions[1]=13; return 2;
    case 'B': actions[0]=10; actions[1]=11; return 2;
    case '9': case 'T': actions[0]=10; actions[1]=11; actions[2]=12; actions[3]=13; return 4;
    case 'D': actions[0]=14; actions[1]=15; actions[2]=16; actions[3]=17; return 4;
    case 'E': if(mouse) return -1; actions[0]=19; actions[1]=20; actions[2]=21; actions[3]=22; return 4;
    case 'G': actions[0]=8; return 1;
    case 'I': actions[0]=9; return 1;
    case 'J': case 'K': actions[0]=8; return 1;
    case 'L': actions[0]=18; return 1;
    case 'M': actions[0]=23; return 1;
    case 'N': actions[0]=10; return 1;
    case 'O': actions[0]=11; return 1;
    case 'P': actions[0]=16; actions[1]=17; return 2;
    case 'Q': actions[0]=14; actions[1]=15; return 2;
    case 'R': if(mouse) return -1; actions[0]=21; actions[1]=22; return 2;
    case 'S': if(mouse) return -1; actions[0]=19; actions[1]=20; return 2;
    default: return 0; /* Includes null glyph 8 and unrelated formatting. */
    }
}

int pc_prompts_format(const char *source, char *out, size_t capacity,
                      const int keys[PC_ACTION_COUNT], int mouse)
{
    size_t used=0;
    int changed=0;
    if (!source || !out || !capacity) return 0;
    for (size_t i=0;source[i];i++) {
        char label[160];
        int actions[4], count=0;
        if (source[i]=='\v' && source[i+1])
            count=prompt_actions((unsigned char)source[i+1],actions,mouse);
        if (count) {
            size_t n=0;
            label[n++]='[';
            if (count<0) { memcpy(label+n,"Mouse",5); n+=5; }
            for (int a=0;a<count;a++) {
                char key[32];
                if (actions[a]<0 || actions[a]>=PC_ACTION_COUNT) return 0;
                key_label(keys[actions[a]],key,sizeof key);
                if (a) label[n++]='/';
                size_t len=strlen(key);
                if (n+len+2>=sizeof label) return 0;
                memcpy(label+n,key,len); n+=len;
            }
            label[n++]=']';
            if (used+n>=capacity) return 0;
            memcpy(out+used,label,n); used+=n;
            i++; changed=1;
        } else {
            if (used+1>=capacity) return 0;
            out[used++]=source[i];
        }
    }
    out[used]=0;
    return changed;
}

static int network_labels(const char *source, char *out, size_t capacity)
{
    size_t used=0;
    int changed=0;
    if(!source || !out || !capacity) return 0;
    for(size_t i=0;source[i];) {
        if((source[i]=='X' || source[i]=='x') && !_strnicmp(source+i,"Xbox Live",9)) {
            if(used+6>=capacity) return 0;
            memcpy(out+used,"ONLINE",6); used+=6; i+=9; changed=1;
        } else {
            if(used+1>=capacity) return 0;
            out[used++]=source[i++];
        }
    }
    out[used]=0;
    return changed;
}

int pc_prompts_rewrite(const char *source, char *out, size_t capacity)
{
    int keys[PC_ACTION_COUNT],enabled,inverted;
    double sensitivity;
    char renamed[2048];
    int changed;
    if(!source || !out || !capacity) return 0;
    /* Menu names change for every input device, including title/breadcrumb
       text. Apply before both text measurement and rendering for alignment. */
    if(pc_storage_rewrite(source,out,capacity))return 1;
    changed=network_labels(source,renamed,sizeof renamed);
    if(changed) source=renamed;
    if(strchr(source,'\v') && pc_prompts_keyboard_active() && pc_settings_prompt_keys(keys)) {
        pc_controls_get_mouse(&sensitivity,&enabled,&inverted);
        if(pc_prompts_format(source,out,capacity,keys,enabled)) return 1;
    }
    if(changed && strlen(source)<capacity) {
        memcpy(out,source,strlen(source)+1);
        return 1;
    }
    return 0;
}
