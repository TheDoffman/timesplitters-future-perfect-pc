/* Included by input_bindings.c: use the runtime's control order, parser,
   and initialization rather than maintaining a second binding vocabulary. */
#include "pc_settings.h"
#include <math.h>
#include "pc_control_presets.h"
static int g_pc_keyboard_preset,g_pc_controller_presets[4],g_pc_custom_semantics;
static PcGameplayMasks g_pc_gameplay_masks;
void pc_settings_gameplay_reader(PcGameplayMasks reader) {
    AcquireSRWLockExclusive(&g_pc_bindings_lock);g_pc_gameplay_masks=reader;ReleaseSRWLockExclusive(&g_pc_bindings_lock);
}
int pc_settings_gameplay_masks(unsigned port,unsigned masks[24],int *mounted) {
    AcquireSRWLockShared(&g_pc_bindings_lock);PcGameplayMasks reader=g_pc_gameplay_masks;ReleaseSRWLockShared(&g_pc_bindings_lock);
    return reader?reader(port,masks,mounted):0;
}
#ifdef TSFP_NATIVE_INPUT
#include "settings_macos.h"
#include "config_files.h"
#include "pc_debug.h"
enum { VK_XBUTTON1=5, VK_XBUTTON2=6, VK_F9=0x78, VK_F10=0x79, VK_F11=0x7a,
       VK_F23=0x86, VK_F24=0x87, VK_LWIN=0x5b, VK_RWIN=0x5c };
#endif
_Static_assert(CONTROL_COUNT == PC_ACTION_COUNT, "PC settings action order must match runtime controls");
static SRWLOCK g_pc_weapon_lock=SRWLOCK_INIT;
static PcWeaponKeys g_pc_weapon_keys;
static unsigned long long now_ms(void);
static int g_pc_developer_enabled;
int pc_weapon_slot_take(void)
{
    int active=pc_controls_weapon_slots_active(),slot;
    AcquireSRWLockExclusive(&g_pc_weapon_lock);
    slot=pc_weapon_keys_take(&g_pc_weapon_keys,active,now_ms());
    ReleaseSRWLockExclusive(&g_pc_weapon_lock);
    return slot;
}

#include "pc_binding_capture.h"
static SRWLOCK g_capture_lock=SRWLOCK_INIT;
static PcBindingCapture g_binding_capture;
void pc_settings_capture_begin(void) {
    AcquireSRWLockExclusive(&g_capture_lock);
    pc_binding_capture_begin(&g_binding_capture,now_ms());
    ReleaseSRWLockExclusive(&g_capture_lock);
}
void pc_settings_capture_key(int key) {
    AcquireSRWLockExclusive(&g_capture_lock);
    pc_binding_capture_key(&g_binding_capture,key);
    ReleaseSRWLockExclusive(&g_capture_lock);
}
void pc_settings_capture_end(void) {
    AcquireSRWLockExclusive(&g_capture_lock);
    if(g_binding_capture.phase)g_binding_capture.phase=3;
    g_binding_capture.event=0;
    ReleaseSRWLockExclusive(&g_capture_lock);
}
int pc_settings_capture_blocked(void) {
    AcquireSRWLockShared(&g_capture_lock);int active=g_binding_capture.phase!=0;
    ReleaseSRWLockShared(&g_capture_lock);return active;
}
int pc_settings_capture_event(void) {
    AcquireSRWLockExclusive(&g_capture_lock);int event=g_binding_capture.event;g_binding_capture.event=0;
    ReleaseSRWLockExclusive(&g_capture_lock);return event;
}
int pc_settings_capture_pump(void) {
    if(!pc_settings_capture_blocked())return 0;
    unsigned char keys[256]={0};int focus=pc_controls_binding_keys(keys);
    AcquireSRWLockExclusive(&g_capture_lock);
    pc_binding_capture_step(&g_binding_capture,keys,focus,now_ms());
    ReleaseSRWLockExclusive(&g_capture_lock);
    return 1; /* Suppress this release sample too. */
}

const char *const pc_action_labels[PC_ACTION_COUNT] = {
    "Interact / confirm (A)", "Melee / back (B)", "Reload (X)", "Y action",
    "Black button action", "White button action", "Pause / Start", "Objectives / Back",
    "Throw grenade", "Fire", "Weapon mode / zoom in", "Grenade type / zoom out",
    "Previous weapon", "Next weapon", "Move forward", "Move backward", "Strafe left",
    "Strafe right", "Crouch", "Look up (optional key)", "Look down (optional key)",
    "Look left (optional key)", "Look right (optional key)", "Aim",
#define PC_WEAPON_SLOT(n,key) "Weapon slot " #n,
#include "pc_weapon_slots.def"
#undef PC_WEAPON_SLOT
    "Developer Tools", "Toggle no-clip", "Toggle free-cam",
};

void pc_settings_defaults(PcSettings *s)
{
    static const int keys[PC_ACTION_COUNT][PC_BINDING_SLOTS] = {
        {'E',VK_SPACE}, {'F',VK_MBUTTON}, {'R'}, {'Y'},
        {'X'}, {'Z'}, {VK_ESCAPE}, {VK_TAB}, {'G',VK_XBUTTON2}, {VK_LBUTTON},
        {VK_UP,VK_PRIOR}, {VK_DOWN,VK_NEXT}, {VK_LEFT,VK_F24}, {VK_RIGHT,VK_F23},
        {'W'}, {'S'}, {'A'}, {'D'}, {VK_CONTROL,'C'}, {0}, {0}, {0}, {0}, {VK_RBUTTON},
#define PC_WEAPON_SLOT(n,key) {key},
#include "pc_weapon_slots.def"
#undef PC_WEAPON_SLOT
        {VK_F1}, {0}, {0},
    };
    memset(s, 0, sizeof *s);
    memcpy(s->keys, keys, sizeof keys);
    s->sensitivity = 0.10; s->enabled = 1;
}

void pc_settings_modern(PcSettings *s)
{
    pc_settings_defaults(s);
#define PC_MODERN_KEY(action,first,second) s->keys[action][0]=first;s->keys[action][1]=second;
#include "pc_control_presets.def"
#undef PC_MODERN_KEY
    s->preset=1;
}

void pc_settings_key_name(int key, char *out, size_t bytes)
{
    const char *name = NULL;
    switch (key) {
    case 0: name = "Unbound"; break;
    case VK_LBUTTON: name = "Mouse 1"; break;
    case VK_RBUTTON: name = "Mouse 2"; break;
    case VK_MBUTTON: name = "Mouse 3"; break;
    case VK_XBUTTON1: name = "Mouse 4"; break;
    case VK_XBUTTON2: name = "Mouse 5"; break;
    case VK_F23: name = "Wheel up"; break;
    case VK_F24: name = "Wheel down"; break;
    }
    if (name) { snprintf(out, bytes, "%s", name); return; }
#ifdef TSFP_NATIVE_INPUT
    tsfp_native_key_name(key,out,bytes);
#else
    if (!GetKeyNameTextA((LONG)(MapVirtualKeyA((UINT)key, MAPVK_VK_TO_VSC) << 16) |
        ((key >= VK_PRIOR && key <= VK_DELETE) ? (1L << 24) : 0), out, (int)bytes))
        snprintf(out, bytes, "Key %d", key);
#endif
}

int pc_settings_conflict(const PcSettings *s, int action, int key)
{
    int i,j;
    if (!key) return -1;
    for (i=0; i<PC_ACTION_COUNT; i++) for (j=0; j<PC_BINDING_SLOTS; j++)
        if (i != action && s->keys[i][j] == key) return i;
    return -1;
}

int pc_settings_key_allowed(int key)
{
    return key>=0 && key<=255 && key!=VK_F2 &&
        key!=VK_F9 && key!=VK_F10 && key!=VK_F11 && key!=VK_F12 &&
        key!=VK_LWIN && key!=VK_RWIN;
}
int pc_settings_assign(PcSettings *s,int action,int slot,int key,int move)
{
    int i,j;
    if(!s || action<0 || action>=PC_ACTION_COUNT || slot<0 || slot>=PC_BINDING_SLOTS ||
       !pc_settings_action_editable(s->preset,action) || !pc_settings_key_allowed(key))return 0;
    int conflict=pc_settings_conflict(s,action,key);
    if(conflict>=0 && !pc_settings_action_editable(s->preset,conflict))return 0;
    if(!move && pc_settings_conflict(s,action,key)>=0)return -1;
    if(key)for(i=0;i<PC_ACTION_COUNT;i++)for(j=0;j<PC_BINDING_SLOTS;j++)
        if(s->keys[i][j]==key)s->keys[i][j]=0;
    s->keys[action][slot]=key;
    return 1;
}

int pc_settings_custom_read(PcSettings *s, char *error, size_t bytes)
{
    int i,j;
    recomp_bindings_init();
    memset(s, 0, sizeof *s);
    if (!g_have_path) { snprintf(error, bytes, "No PC controls configuration was loaded. Start the game with its launcher."); return 0; }
    AcquireSRWLockShared(&g_pc_bindings_lock);
    if (g_ctl[0].device != DEV_KEYBOARD) {
        ReleaseSRWLockShared(&g_pc_bindings_lock);
        snprintf(error, bytes, "Player 1 is using a controller profile. Select the PC keyboard profile before editing keyboard bindings.");
        return 0;
    }
    for (i=0; i<CONTROL_COUNT; i++) for (j=0; j<g_ctl[0].bind[i].count && j<PC_BINDING_SLOTS; j++) {
        if (g_ctl[0].bind[i].src[j].kind == SRC_KEY) s->keys[i][j] = g_ctl[0].bind[i].src[j].arg;
    }
    ReleaseSRWLockShared(&g_pc_bindings_lock);
    pc_controls_get_mouse(&s->sensitivity, &s->enabled, &s->invert_y);
    return 1;
}

int pc_settings_read(PcSettings *s,char *error,size_t bytes)
{
    if(!pc_settings_custom_read(s,error,bytes))return 0;
    AcquireSRWLockShared(&g_pc_bindings_lock);int modern=g_pc_keyboard_preset;ReleaseSRWLockShared(&g_pc_bindings_lock);
    if(modern) {PcSettings custom=*s;pc_settings_modern(s);
        s->sensitivity=custom.sensitivity;s->enabled=custom.enabled;s->invert_y=custom.invert_y;
        for(int a=0;a<PC_ACTION_COUNT;a++)if((a>=19 && a<=22) || a>=PC_DEVELOPER_ACTION)
            memcpy(s->keys[a],custom.keys[a],sizeof s->keys[a]);}
    return 1;
}
int pc_settings_custom_semantics(void)
{ recomp_bindings_init();AcquireSRWLockShared(&g_pc_bindings_lock);int active=g_pc_custom_semantics;ReleaseSRWLockShared(&g_pc_bindings_lock);return active; }

int pc_settings_controller_preset(unsigned port)
{
    recomp_bindings_init();if(port>=4)return 0;
    AcquireSRWLockShared(&g_pc_bindings_lock);int modern=g_pc_controller_presets[port];ReleaseSRWLockShared(&g_pc_bindings_lock);return modern;
}

/* The first assigned key is the compact in-game prompt; alternatives remain
   available in Keyboard & Mouse settings. */
int pc_settings_prompt_keys(int keys[PC_ACTION_COUNT])
{
    int i,j,available=0;
    recomp_bindings_init();
    memset(keys,0,PC_ACTION_COUNT*sizeof *keys);
    AcquireSRWLockShared(&g_pc_bindings_lock);
    for(i=0;i<CONTROL_COUNT;i++) for(j=0;j<g_ctl[0].bind[i].count;j++) {
        if(g_ctl[0].bind[i].src[j].kind==SRC_KEY) {
            keys[i]=g_ctl[0].bind[i].src[j].arg;
            available=1; break;
        }
    }
    ReleaseSRWLockShared(&g_pc_bindings_lock);
    AcquireSRWLockShared(&g_pc_bindings_lock);int modern=g_pc_keyboard_preset,semantic=g_ctl[0].device==DEV_KEYBOARD;ReleaseSRWLockShared(&g_pc_bindings_lock);
    if(modern || semantic) {int custom_keys[PC_ACTION_COUNT];memcpy(custom_keys,keys,sizeof custom_keys);PcSettings preset;pc_settings_modern(&preset);for(i=0;i<PC_ACTION_COUNT;i++)if(modern && !(i>=19 && i<=22) && i<PC_DEVELOPER_ACTION)keys[i]=preset.keys[i][0];available=1;
        if(pc_controls_in_menu()) {if(modern) {keys[0]=13;keys[1]=27;keys[10]=38;keys[11]=40;keys[12]=37;keys[13]=39;}}
        else {unsigned masks[24];int mounted,semantic_keys[PC_ACTION_COUNT];memcpy(masks,pc_preset_masks,sizeof masks);
            memcpy(semantic_keys,keys,sizeof semantic_keys);
            if(pc_settings_gameplay_masks(0,masks,&mounted))for(unsigned raw=0;raw<24;raw++) {
                unsigned action=pc_preset_semantic(raw,masks);keys[raw]=semantic_keys[action];
            }else memcpy(keys,custom_keys,sizeof custom_keys);}
    }
    return available;
}

/* Find a value in a JSON object without touching unrelated members. The
   runtime JSON walker also handles strings, escapes and nested arrays. */
static const char *pc_member(const char *object, const char *wanted, const char **end)
{
    const char *p = ws(object), *value, *next;
    char name[64];
    if (*p++ != '{') return NULL;
    p = ws(p);
    while (*p && *p != '}') {
        p = jstring(p, name, sizeof name);
        if (!p || *(p = ws(p)) != ':') return NULL;
        value = ws(p + 1); next = jskip(value);
        if (!next) return NULL;
        if (!strcmp(name, wanted)) { if (end) *end = next; return value; }
        p = ws(next);
        if (*p == ',') p = ws(p + 1);
        else if (*p != '}') return NULL;
    }
    return NULL;
}

static const char *pc_controller_port(const char *text,int wanted)
{
    const char *p = pc_member(text, "controllers", NULL), *port;
    int index=0;
    if (!p || *p++ != '[') return NULL;
    p = ws(p);
    while (*p && *p != ']') {
        const char *next = jskip(p);
        if (!next || *p != '{') return NULL;
        port = pc_member(p, "port", NULL);
        if ((port && strtol(port,NULL,10) == wanted+1) || (!port && index == wanted)) return p;
        p = ws(next); if (*p == ',') p = ws(p+1);
        index++;
    }
    return NULL;
}

static const char *pc_controller(const char *text) {return pc_controller_port(text,0);}

static int pc_replace_member(char **text, const char *object, const char *name, const char *value)
{
    const char *end, *start = pc_member(object, name, &end);
    char addition[96], *result;
    size_t before, after, middle;
    if (start) addition[0] = 0;
    else {
        end = jskip(object);
        if (!end) return 0;
        start = --end;
        snprintf(addition, sizeof addition, "%s\"%s\":", *ws(object+1) == '}' ? "" : ",", name);
    }
    before = (size_t)(start-*text); after = strlen(end); middle = strlen(addition)+strlen(value);
    result = malloc(before+middle+after+1);
    if (!result) return 0;
    memcpy(result,*text,before);
    snprintf(result+before,middle+1,"%s%s",addition,value);
    memcpy(result+before+middle,end,after+1);
    free(*text); *text = result;
    return 1;
}

static int pc_write_config(const char *text)
{
#ifdef TSFP_NATIVE_INPUT
    return tsfp_config_write(g_path,text,1);
#else
    char temp[600], backup[600];
    FILE *f;
    int ok;
    snprintf(temp,sizeof temp,"%s.%lu.tmp",g_path,GetCurrentProcessId());
    snprintf(backup,sizeof backup,"%s.bak",g_path);
    if (GetFileAttributesA(g_path) == INVALID_FILE_ATTRIBUTES ||
        (GetFileAttributesA(g_path) & FILE_ATTRIBUTE_READONLY)) {
        SetLastError(ERROR_ACCESS_DENIED); return 0;
    }
    f=fopen(temp,"wb");
    if (!f) return 0;
    ok=fwrite(text,1,strlen(text),f)==strlen(text);
    if (fclose(f)) ok=0;
    if (!ok || !CopyFileA(g_path,backup,FALSE) ||
        !MoveFileExA(temp,g_path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        DWORD error=GetLastError(); DeleteFileA(temp); SetLastError(error); return 0;
    }
    return 1;
#endif
}

int pc_settings_developer_enabled(void)
{
    recomp_bindings_init();
    AcquireSRWLockShared(&g_pc_bindings_lock);int enabled=g_pc_developer_enabled;
    ReleaseSRWLockShared(&g_pc_bindings_lock);return enabled;
}
void pc_settings_developer_keys(int keys[PC_BINDING_SLOTS])
{ pc_settings_action_keys(PC_DEVELOPER_ACTION,keys); }
void pc_settings_action_keys(int action,int keys[PC_BINDING_SLOTS])
{
    recomp_bindings_init();memset(keys,0,PC_BINDING_SLOTS*sizeof *keys);
    AcquireSRWLockShared(&g_pc_bindings_lock);
    if(action<0 || action>=PC_ACTION_COUNT){ReleaseSRWLockShared(&g_pc_bindings_lock);return;}
    const Binding *binding=&g_ctl[0].bind[action];
    for(int i=0;i<binding->count && i<PC_BINDING_SLOTS;i++)
        if(binding->src[i].kind==SRC_KEY)keys[i]=binding->src[i].arg;
    ReleaseSRWLockShared(&g_pc_bindings_lock);
}
int pc_settings_developer_save(int enabled,char *error,size_t bytes)
{
    recomp_bindings_init();
    char *text=g_have_path?read_file(g_path):NULL;
    int ok=text && pc_replace_member(&text,ws(text),"developer_tools_enabled",enabled?"true":"false") && pc_write_config(text);
    free(text);
    if(!ok){snprintf(error,bytes,"Could not save Developer Tools. Your previous setting was kept.");return 0;}
    AcquireSRWLockExclusive(&g_pc_bindings_lock);g_pc_developer_enabled=!!enabled;
    ReleaseSRWLockExclusive(&g_pc_bindings_lock);return 1;
}

/* Public local-player assignments: -2 keyboard, -1 disabled, 0..3 pad. */
#include "pc_local_join.h"
static PcLocalJoin g_players_join;
static unsigned g_players_auto_ports; /* Visible, but not claimed by another player yet. */
static int g_players_preferred[4]={-1,-1,-1,-1};
static void pc_settings_players_detect(void)
{
    int devices[4];unsigned connected=0;
    for(int i=0;i<4;i++) {RecompPadState pad;if(recomp_pad_read(i,&pad))connected|=1u<<i;}
    AcquireSRWLockExclusive(&g_pc_bindings_lock);
    for(int i=0;i<4;i++) {
        devices[i]=g_ctl[i].device==DEV_KEYBOARD?-2:g_ctl[i].device==DEV_PAD?g_ctl[i].pad:-1;
        if(devices[i]>=0)g_players_preferred[i]=devices[i];
    }
    pc_local_detect(devices,g_players_preferred,connected);
    for(int i=1;i<4;i++)if(g_ctl[i].device==DEV_NONE && devices[i]>=0) {
        Controller *c=&g_ctl[i];c->device=DEV_PAD;c->pad=devices[i];
        int has_pad=0;
        for(int k=0;k<CONTROL_COUNT;k++)for(int b=0;b<c->bind[k].count;b++)
            if(c->bind[k].src[b].kind!=SRC_KEY && c->bind[k].src[b].kind!=SRC_NONE)has_pad=1;
        c->pad_fallback=c->has_key&&!has_pad;
        g_players_auto_ports|=1u<<i;
        g_players_preferred[i]=devices[i];describe(c,c->label,sizeof c->label);
    }
    ReleaseSRWLockExclusive(&g_pc_bindings_lock);
}

int pc_settings_players_join(unsigned used_ports)
{
    recomp_bindings_init();
    int devices[4];unsigned starts=0;unsigned char keys[256];
    pc_settings_players_read(devices);
    if(!pc_controls_binding_keys(keys)) {
        g_players_join.held=15;g_players_join.pending=0;return -1;
    }
    for(int i=0;i<4;i++) {
        RecompPadState pad;
        if(recomp_pad_read(i,&pad) && (pad.buttons&RECOMP_PAD_START))starts|=1u<<i;
    }
    if(used_ports&~15u) {
        g_players_join.held=starts;g_players_join.pending=0;g_players_join.initialized=1;
        return -1;
    }
    int port=pc_local_join(&g_players_join,devices,starts,used_ports);
    if(port<0)return -1;
    AcquireSRWLockExclusive(&g_pc_bindings_lock);
    g_players_auto_ports&=~(1u<<port);
    for(int i=0;i<4;i++)if(devices[i]>=0 && (g_ctl[i].device!=DEV_PAD || g_ctl[i].pad!=devices[i])) {
        Controller *c=&g_ctl[i];int has_pad=0;
        c->device=DEV_PAD;c->pad=devices[i];
        for(int k=0;k<CONTROL_COUNT;k++)for(int b=0;b<c->bind[k].count;b++)
            if(c->bind[k].src[b].kind!=SRC_KEY && c->bind[k].src[b].kind!=SRC_NONE)has_pad=1;
        c->pad_fallback=c->has_key&&!has_pad;describe(c,c->label,sizeof c->label);
    }
    ReleaseSRWLockExclusive(&g_pc_bindings_lock);
    return port;
}
int pc_settings_keyboard_player(void)
{
    int active;recomp_bindings_init();AcquireSRWLockShared(&g_pc_bindings_lock);
    active=g_ctl[0].device==DEV_KEYBOARD;ReleaseSRWLockShared(&g_pc_bindings_lock);return active;
}
void pc_settings_players_read(int devices[4])
{
    recomp_bindings_init();AcquireSRWLockShared(&g_pc_bindings_lock);
    for(int i=0;i<4;i++)devices[i]=g_ctl[i].device==DEV_KEYBOARD?-2:g_ctl[i].device==DEV_PAD?g_ctl[i].pad:-1;
    ReleaseSRWLockShared(&g_pc_bindings_lock);
}
int pc_settings_players_save(const int devices[4],char *error,size_t bytes)
{
    unsigned used=0;char *text=NULL;int ok=0;
    recomp_bindings_init();
    for(int i=0;i<4;i++) {
        int d=devices[i];
        if(d < -2 || d>3 || (i && d==-2) || (!i && d==-1)) {
            snprintf(error,bytes,"Player 1 needs a device; keyboard and mouse belong to Player 1.");return 0;
        }
        if(d>=0 && (used&(1u<<d))) {
            snprintf(error,bytes,"Assign each controller to only one player.");return 0;
        }
        if(d>=0)used|=1u<<d;
    }
    text=g_have_path?read_file(g_path):NULL;if(!text)goto fail;
    /* Ambiguous profiles cannot be safely edited: the normal loader could
     * otherwise select a different duplicate after the next restart. */
    {
        const char *p=pc_member(text,"controllers",NULL);unsigned ports=0;int index=0;
        if(!p || *p++!='[')goto fail;
        p=ws(p);
        while(*p && *p!=']') {
            const char *next=jskip(p),*port=pc_member(p,"port",NULL);char *tail;
            long slot=port?strtol(port,&tail,10)-1:index;
            if(!next || *p!='{' || slot<0 || slot>3 || (ports&(1u<<slot)))goto fail;
            if(port && *ws(tail)!=',' && *ws(tail)!='}')goto fail;
            ports|=1u<<slot;index++;p=ws(next);if(*p==',')p=ws(p+1);
        }
        if(*p!=']')goto fail;
    }
    for(int i=0;i<4;i++) {
        const char *profile=pc_controller_port(text,i);char value[40];
        if(!profile) {
            const char *end,*array=pc_member(text,"controllers",&end);
            if(!array || *array!='[' || end<=array || end[-1]!=']')goto fail;
            size_t n=(size_t)(end-array)-1;char *expanded=malloc(n+64);
            if(!expanded)goto fail;
            memcpy(expanded,array,n);
            snprintf(expanded+n,64,"%s{\"port\":%d,\"device\":\"none\"}]",*ws(array+1)==']'?"":",",i+1);
            int changed=pc_replace_member(&text,ws(text),"controllers",expanded);free(expanded);
            if(!changed)goto fail;
            profile=pc_controller_port(text,i);if(!profile)goto fail;
        }
        if(devices[i]>=0)snprintf(value,sizeof value,"\"gamepad:%d\"",devices[i]);
        else snprintf(value,sizeof value,"\"%s\"",devices[i]==-2?"keyboard":"none");
        if(!pc_replace_member(&text,profile,"device",value))goto fail;
    }
    /* Validate with the normal parser before touching disk or publishing.
     * Its destination is global, so keep the temporary state private under
     * the same lock used by the sampler, then restore it before file I/O. */
    Controller before[4],next[4];int valid=1;
    AcquireSRWLockExclusive(&g_pc_bindings_lock);memcpy(before,g_ctl,sizeof before);
    for(int i=0;i<4;i++) {
        int bad=0;
        if(!parse_controller(pc_controller_port(text,i),i,&bad) || bad)valid=0;
        describe(&g_ctl[i],g_ctl[i].label,sizeof g_ctl[i].label);
    }
    memcpy(next,g_ctl,sizeof next);memcpy(g_ctl,before,sizeof before);
    ReleaseSRWLockExclusive(&g_pc_bindings_lock);
    if(!valid || !pc_write_config(text))goto fail;
    for(int i=0;i<4;i++)if(before[i].device==DEV_PAD)recomp_pad_rumble(before[i].pad,0,0);
    AcquireSRWLockExclusive(&g_pc_bindings_lock);
    for(int i=0;i<4;i++)if(before[i].device==DEV_PAD)g_players_preferred[i]=before[i].pad;
    memcpy(g_ctl,next,sizeof next);g_players_auto_ports=0;
    ReleaseSRWLockExclusive(&g_pc_bindings_lock);
    ok=1;
fail:
    free(text);if(!ok)snprintf(error,bytes,"Could not save player devices. Active assignments were kept.");
    return ok;
}
int pc_settings_pad_status(int slot,char *name,size_t bytes,int *pressed)
{
    RecompPadState pad;*pressed=0;
    if(slot<0 || slot>=4 || !recomp_pad_read(slot,&pad)) {snprintf(name,bytes,"Disconnected");return 0;}
    recomp_pad_name(slot,name,bytes);
    *pressed=pad.buttons!=0;return 1;
}

static int pc_debug_bool(const char *object, const char *name)
{
    const char *end, *value = pc_member(object,name,&end);
    return value && end-value>=4 && !strncmp(value,"true",4) && ws(value+4)==end;
}

int pc_settings_menu_visible(void)
{
    char *text;
    int visible;
    recomp_bindings_init();
    text=g_have_path?read_file(g_path):NULL;
    visible=text && pc_debug_bool(text,"pc_menu_visible");
    free(text);
    return visible;
}

int pc_settings_save_menu_visible(int visible)
{
    char *text;
    int ok;
    recomp_bindings_init();
    text=g_have_path?read_file(g_path):NULL;
    if(!text) return 0;
    ok=*ws(text)=='{' && pc_replace_member(&text,ws(text),"pc_menu_visible",
                                          visible?"true":"false") && pc_write_config(text);
    free(text);
    return ok;
}

void pc_debug_config_read(PcDebugOptions *s)
{
    char *text;
    const char *debug, *env;
    memset(s,0,sizeof *s);
    recomp_bindings_init();
    text=g_have_path?read_file(g_path):NULL;
    if (text && (debug=pc_member(text,"debug",NULL)) && *debug=='{') {
        for(unsigned i=0;i<PC_DEBUG_OPTION_COUNT;i++)
            pc_debug_option_set(s,i,pc_debug_bool(debug,pc_debug_option(i)->key));
    }
    free(text);
    env=getenv("RECOMP_FPS_OVERLAY");
    if (env && *env) s->show_fps=strcmp(env,"0")!=0;
}

int pc_debug_config_save(const PcDebugOptions *s, char *error, size_t bytes)
{
    char *text;
    const char *debug;
    int ok=0;
    recomp_bindings_init();
    if (!g_have_path) { snprintf(error,bytes,"No settings file is loaded. Start the game with its launcher."); return 0; }
    text=read_file(g_path);
    if (!text) goto failure;
    debug=pc_member(text,"debug",NULL);
    if (!debug || *debug!='{') {
        if (!pc_replace_member(&text,ws(text),"debug","{}")) goto failure;
    }
    for(unsigned i=0;i<PC_DEBUG_OPTION_COUNT;i++)
        if(!pc_replace_member(&text,pc_member(text,"debug",NULL),pc_debug_option(i)->key,
            pc_debug_option_value(s,i)?"true":"false")) goto failure;
    if(!pc_write_config(text)) goto failure;
    pc_debug_set(s);
    ok=1;
failure:
    free(text);
    if (!ok) snprintf(error,bytes,"Could not safely save debug settings (system error %lu). Active settings were kept.",(unsigned long)GetLastError());
    return ok;
}

int pc_settings_deadzone(unsigned port,int aim)
{
    recomp_bindings_init();if(port>=4)return 7849;
    AcquireSRWLockShared(&g_pc_bindings_lock);
    int value=aim?g_ctl[port].aim_deadzone:g_ctl[port].move_deadzone;
    if(value<0)value=g_ctl[port].deadzone;
    ReleaseSRWLockShared(&g_pc_bindings_lock);
    return value<0?0:value>32767?32767:value;
}
int pc_settings_deadzone_save(unsigned port,int aim,int value,char *error,size_t bytes)
{
    recomp_bindings_init();if(port>=4 || value<0 || value>32767)return 0;
    char number[16];snprintf(number,sizeof number,"%d",value);
    char *text=g_have_path?read_file(g_path):NULL;
    const char *profile=text?pc_controller_port(text,(int)port):NULL;
    if(!profile || !pc_replace_member(&text,profile,aim?"aim_deadzone":"move_deadzone",number) || !pc_write_config(text)) {
        free(text);snprintf(error,bytes,"Could not save deadzone. Active controls were kept.");return 0;
    }
    free(text);AcquireSRWLockExclusive(&g_pc_bindings_lock);
    if(aim)g_ctl[port].aim_deadzone=value;else g_ctl[port].move_deadzone=value;
    ReleaseSRWLockExclusive(&g_pc_bindings_lock);return 1;
}

int pc_settings_controller_preset_save(unsigned port,int modern,char *error,size_t bytes)
{
    recomp_bindings_init();if(port>=4 || (modern!=0 && modern!=1))return 0;
    char *text=g_have_path?read_file(g_path):NULL;
    const char *profile=text?pc_controller_port(text,(int)port):NULL;
    if(!profile || !pc_replace_member(&text,profile,"controller_preset",modern?"1":"0") || !pc_write_config(text)) {
        free(text);snprintf(error,bytes,"Could not save the controller preset. Your controls were kept.");return 0;}
    free(text);AcquireSRWLockExclusive(&g_pc_bindings_lock);g_pc_controller_presets[port]=modern;ReleaseSRWLockExclusive(&g_pc_bindings_lock);return 1;
}

int pc_settings_save(const PcSettings *s, char *error, size_t bytes)
{
    char value[128],*text=NULL;
    const char *controller,*mouse;
    int changed[PC_ACTION_COUNT]={0};
    PcSettings current;
    if (!pc_settings_custom_read(&current,error,bytes)) return 0;
    if (!isfinite(s->sensitivity) || s->sensitivity < .001 || s->sensitivity > 10) {
        snprintf(error,bytes,"Mouse sensitivity must be between 0.001 and 10."); return 0;
    }
    if(s->preset!=0 && s->preset!=1) {
        snprintf(error,bytes,"Choose Custom or Modern PC.");return 0;
    }
    for(int i=0;i<PC_ACTION_COUNT;i++) {
        for(int j=0;j<PC_BINDING_SLOTS;j++)if(!pc_settings_key_allowed(s->keys[i][j])) {
            snprintf(error,bytes,"A binding uses an invalid key, a system key, or a reserved function key (F2, F9, F10, F11, F12).");return 0;
        }
        changed[i]=pc_settings_action_editable(s->preset,i) &&
            memcmp(s->keys[i],current.keys[i],sizeof s->keys[i])!=0;
    }
    text=read_file(g_path);
    if(!text || !(controller=pc_controller(text)))goto failure;
    for(int i=0;i<PC_ACTION_COUNT;i++)if(changed[i]) {
        /* Replace only the edited action; preserve other legacy sources and fields. */
        const char *bindings=pc_member(controller,"bindings",NULL);
        if(!bindings || *bindings!='{') {
            if(!pc_replace_member(&text,controller,"bindings","{}"))goto failure;
        }
        size_t used=1;value[0]='[';
        for(int j=0;j<PC_BINDING_SLOTS;j++) {
            int key=s->keys[i][j];
            if(key)used+=(size_t)snprintf(value+used,sizeof value-used,"%s\"key:#%d\"",j?",":"",key);
            else used+=(size_t)snprintf(value+used,sizeof value-used,"%s\"none\"",j?",":"");
        }
        snprintf(value+used,sizeof value-used,"]");
        if(!pc_replace_member(&text,pc_member(pc_controller(text),"bindings",NULL),CONTROLS[i].name,value))goto failure;
        controller=pc_controller(text);
    }
    if(!pc_replace_member(&text,pc_controller(text),"keyboard_preset",s->preset?"1":"0"))goto failure;
    mouse=pc_member(text,"mouse",NULL);
    if(!mouse || *mouse!='{') {
        if(!pc_replace_member(&text,ws(text),"mouse","{}"))goto failure;
    }
    snprintf(value,sizeof value,"%.9g",s->sensitivity);
    if(!pc_replace_member(&text,pc_member(text,"mouse",NULL),"sensitivity",value) ||
       !pc_replace_member(&text,pc_member(text,"mouse",NULL),"enabled",s->enabled?"true":"false") ||
       !pc_replace_member(&text,pc_member(text,"mouse",NULL),"invert_y",s->invert_y?"true":"false"))goto failure;
    if(!pc_write_config(text))goto failure;
    AcquireSRWLockExclusive(&g_pc_bindings_lock);
    for(int i=0;i<CONTROL_COUNT;i++)if(changed[i]) {
        Binding *binding=&g_ctl[0].bind[i];memset(binding,0,sizeof *binding);
        binding->count=PC_BINDING_SLOTS;
        for(int j=0;j<PC_BINDING_SLOTS;j++)if(s->keys[i][j]) {
            binding->src[j].kind=SRC_KEY;binding->src[j].arg=(short)s->keys[i][j];
        }
    }
    g_pc_keyboard_preset=s->preset;
    ReleaseSRWLockExclusive(&g_pc_bindings_lock);
    AcquireSRWLockExclusive(&g_pc_weapon_lock);
    memset(&g_pc_weapon_keys,0,sizeof g_pc_weapon_keys);
    ReleaseSRWLockExclusive(&g_pc_weapon_lock);
    pc_controls_set_mouse(s->sensitivity,s->enabled,s->invert_y);
    fprintf(stderr,"[PC-SETTINGS] Saved and applied keyboard/mouse settings to %s\n",g_path);
    free(text);return 1;
failure:
    free(text);
    snprintf(error,bytes,"Could not safely save the controls file (system error %lu). Your active bindings were kept.",(unsigned long)GetLastError());
    return 0;
}

/* Only missing actions receive defaults. Keep explicit Unbound and custom
 * assignments. Old stock White=1/Black=2 are superseded; no disk write occurs
 * until the player saves. The launcher uses the same migration policy. */
static void pc_weapon_bindings_migrate(const char *text)
{
    const char *controller=pc_controller(text);
    const char *preset=controller?pc_member(controller,"keyboard_preset",NULL):NULL;
    g_pc_keyboard_preset=preset && *preset=='1';
    const char *semantics=pc_member(text,"custom_binding_semantics",NULL);
    g_pc_custom_semantics=semantics && *semantics=='1';
    for(int port=0;port<4;port++) {const char *profile=pc_controller_port(text,port);
        preset=profile?pc_member(profile,"controller_preset",NULL):NULL;
        g_pc_controller_presets[port]=preset && *preset=='1';}
    const char *bindings=controller?pc_member(controller,"bindings",NULL):NULL;
    Controller *c=&g_ctl[0];
    const char *enabled=pc_member(text,"developer_tools_enabled",NULL);
    g_pc_developer_enabled=enabled && !strncmp(enabled,"true",4);
    if(!bindings || !pc_member(bindings,"developer_tools",NULL)) {
        Binding *binding=&c->bind[PC_DEVELOPER_ACTION];memset(binding,0,sizeof *binding);
        int used=0;
        for(int i=0;i<PC_DEVELOPER_ACTION;i++)for(int j=0;j<c->bind[i].count;j++)
            if(c->bind[i].src[j].kind==SRC_KEY && c->bind[i].src[j].arg==VK_F1)used=1;
        if(!used){binding->count=1;binding->src[0].kind=SRC_KEY;binding->src[0].arg=VK_F1;}
    }
    if(!bindings || c->device!=DEV_KEYBOARD)return;
    if(!pc_member(bindings,"weapon_slot_1",NULL)) {
        const int actions[2]={5,4},keys[2]={'1','2'};
        for(int n=0;n<2;n++) {
            Binding *b=&c->bind[actions[n]];
            if(b->count==1 && b->src[0].kind==SRC_KEY && b->src[0].arg==keys[n]) {
                int replacement=n?'X':'Z',used=0;
                for(int a=0;a<CONTROL_COUNT;a++)for(int j=0;j<c->bind[a].count;j++)
                    if(c->bind[a].src[j].kind==SRC_KEY && c->bind[a].src[j].arg==replacement)used=1;
                memset(b,0,sizeof *b);
                if(!used) {b->count=1;b->src[0].kind=SRC_KEY;b->src[0].arg=(short)replacement;}
            }
        }
    }
    for(int i=0;i<PC_WEAPON_SLOT_COUNT;i++) {
        int action=PC_PAD_ACTION_COUNT+i,key=i<9?'1'+i:i==9?'0':0,used=0;
        if(pc_member(bindings,CONTROLS[action].name,NULL))continue;
        for(int a=0;a<CONTROL_COUNT;a++)for(int j=0;j<c->bind[a].count;j++)
            if(c->bind[a].src[j].kind==SRC_KEY && c->bind[a].src[j].arg==key)used=1;
        if(key && !used) {
            Binding *b=&c->bind[action];
            b->count=1;b->src[0].kind=SRC_KEY;b->src[0].arg=(short)key;
        }
    }
}
