/* Included by input_bindings.c: use the runtime's control order, parser,
   and initialization rather than maintaining a second binding vocabulary. */
#include "pc_settings.h"
#include <math.h>
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
    };
    memset(s, 0, sizeof *s);
    memcpy(s->keys, keys, sizeof keys);
    s->sensitivity = 0.10; s->enabled = 1;
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
    return key>=0 && key<=255 && key!=VK_F1 && key!=VK_F2 &&
        key!=VK_F9 && key!=VK_F10 && key!=VK_F11 && key!=VK_F12 &&
        key!=VK_LWIN && key!=VK_RWIN;
}
int pc_settings_assign(PcSettings *s,int action,int slot,int key,int move)
{
    int i,j;
    if(!s || action<0 || action>=PC_ACTION_COUNT || slot<0 || slot>=PC_BINDING_SLOTS ||
       !pc_settings_key_allowed(key))return 0;
    if(!move && pc_settings_conflict(s,action,key)>=0)return -1;
    if(key)for(i=0;i<PC_ACTION_COUNT;i++)for(j=0;j<PC_BINDING_SLOTS;j++)
        if(s->keys[i][j]==key)s->keys[i][j]=0;
    s->keys[action][slot]=key;
    return 1;
}

int pc_settings_read(PcSettings *s, char *error, size_t bytes)
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

static const char *pc_controller(const char *text)
{
    const char *p = pc_member(text, "controllers", NULL), *port;
    int index=0;
    if (!p || *p++ != '[') return NULL;
    p = ws(p);
    while (*p && *p != ']') {
        const char *next = jskip(p);
        if (!next || *p != '{') return NULL;
        port = pc_member(p, "port", NULL);
        if ((port && strtol(port,NULL,10) == 1) || (!port && index == 0)) return p;
        p = ws(next); if (*p == ',') p = ws(p+1);
        index++;
    }
    return NULL;
}

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

int pc_settings_save(const PcSettings *s, char *error, size_t bytes)
{
    char bindings[16384]="{", value[128];
    char *text = NULL;
    const char *controller, *mouse;
    size_t used=1;
    int i,j;
    PcSettings current;
    if (!pc_settings_read(&current,error,bytes)) return 0;
    if (!isfinite(s->sensitivity) || s->sensitivity < .001 || s->sensitivity > 10) {
        snprintf(error,bytes,"Mouse sensitivity must be between 0.001 and 10."); return 0;
    }
    for (i=0;i<PC_ACTION_COUNT;i++) {
        used += (size_t)snprintf(bindings+used,sizeof bindings-used,"%s\"%s\":[",i?",":"",CONTROLS[i].name);
        for (j=0;j<PC_BINDING_SLOTS;j++) {
            int key=s->keys[i][j];
            if (!pc_settings_key_allowed(key)) {
                snprintf(error,bytes,"A binding uses an invalid key, a system key, or a reserved function key (F1, F2, F9, F10, F11, F12)."); return 0;
            }
            if (key) used += (size_t)snprintf(bindings+used,sizeof bindings-used,"%s\"key:#%d\"",j?",":"",key);
            else used += (size_t)snprintf(bindings+used,sizeof bindings-used,"%s\"none\"",j?",":"");
        }
        used += (size_t)snprintf(bindings+used,sizeof bindings-used,"]");
    }
    snprintf(bindings+used,sizeof bindings-used,"}");
    text=read_file(g_path);
    if (!text || !(controller=pc_controller(text)) ||
        !pc_replace_member(&text,controller,"bindings",bindings)) goto failure;
    mouse=pc_member(text,"mouse",NULL);
    if (!mouse || *mouse != '{') {
        if (!pc_replace_member(&text,ws(text),"mouse","{}")) goto failure;
    }
    snprintf(value,sizeof value,"%.9g",s->sensitivity);
    if (!pc_replace_member(&text,pc_member(text,"mouse",NULL),"sensitivity",value) ||
        !pc_replace_member(&text,pc_member(text,"mouse",NULL),"enabled",s->enabled?"true":"false") ||
        !pc_replace_member(&text,pc_member(text,"mouse",NULL),"invert_y",s->invert_y?"true":"false")) goto failure;
    if (!pc_write_config(text)) goto failure;
    AcquireSRWLockExclusive(&g_pc_bindings_lock);
    for (i=0;i<CONTROL_COUNT;i++) {
        Binding *b=&g_ctl[0].bind[i];
        memset(b,0,sizeof *b);
        b->count=PC_BINDING_SLOTS;
        for (j=0;j<PC_BINDING_SLOTS;j++) if (s->keys[i][j]) {
            b->src[j].kind=SRC_KEY; b->src[j].arg=(short)s->keys[i][j];
        }
    }
    ReleaseSRWLockExclusive(&g_pc_bindings_lock);
    AcquireSRWLockExclusive(&g_pc_weapon_lock);
    memset(&g_pc_weapon_keys,0,sizeof g_pc_weapon_keys);
    ReleaseSRWLockExclusive(&g_pc_weapon_lock);
    pc_controls_set_mouse(s->sensitivity,s->enabled,s->invert_y);
    fprintf(stderr,"[PC-SETTINGS] Saved and applied keyboard/mouse settings to %s\n",g_path);
    free(text); return 1;
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
    const char *bindings=controller?pc_member(controller,"bindings",NULL):NULL;
    Controller *c=&g_ctl[0];
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
