/* Profile-local access to locked content. The original profile, medals and
 * mission records are never modified by this option. */
#define UNLOCK_ALL_TOGGLE_CB 0x1237bu
#define UNLOCK_ALL_TEXT_BYTES 48u
extern int xbox_save_root_utf8(char *out, unsigned capacity);
static uint32_t unlock_all_text;
static uint32_t unlock_all_menu;
static char unlock_all_path[4200];
static int unlock_all_loaded, unlock_all_value;

static FILE *unlock_all_open(const char *path, const char *mode)
{
#ifdef _WIN32
    WCHAR wide[4200], wide_mode[8];
    FILE *file=NULL;
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,4200) ||
       !MultiByteToWideChar(CP_UTF8,0,mode,-1,wide_mode,8) ||
       _wfopen_s(&file,wide,wide_mode)) return NULL;
    return file;
#else
    return fopen(path,mode);
#endif
}

static int unlock_all_identity(char *out, size_t capacity)
{
    char root[4096];
    uint32_t index=WORD_AT(0x0078AD34u), profile, name;
    uint64_t hash=UINT64_C(14695981039346656037);
    size_t length=0;
    if(index>=16 || !xbox_save_root_utf8(root,sizeof root)) return 0;
    /* sub_00069720 resolves this slot to profile+4. Read the same pointer
       directly: calling guest code during a menu eligibility check changes
       the active guest stack and can abort the profile transition. */
    profile=WORD_AT(0x007BEDC0u+index*0x1F54u);
    name=profile ? profile+4u : 0u;
    if(!name || !guest_writable(name,64)) return 0;
    const unsigned char *text=(const unsigned char *)((uintptr_t)name+g_xbox_mem_offset);
    for(;length<64 && text[length];length++) {
        hash^=text[length];hash*=UINT64_C(1099511628211);
    }
    if(!length || length==64) return 0;
    int count=snprintf(out,capacity,"%s/UserData/45410066/pc-unlock-all-%016llx.ini",
                       root,(unsigned long long)hash);
    return count>0 && (size_t)count<capacity;
}

static void unlock_all_caption(void)
{
    if(!unlock_all_text) {
        unlock_all_text=xbox_HeapAlloc(UNLOCK_ALL_TEXT_BYTES,4);
        if(!unlock_all_text)return;
    }
    snprintf((char *)((uintptr_t)unlock_all_text+g_xbox_mem_offset),
             UNLOCK_ALL_TEXT_BYTES,"Unlock All: %s",unlock_all_value?"ON":"OFF");
}

static int unlock_all_sync(void)
{
    char path[sizeof unlock_all_path], line[40];
    if(!unlock_all_identity(path,sizeof path)) {
        unlock_all_loaded=0;unlock_all_value=0;return 0;
    }
    if(unlock_all_loaded && !strcmp(unlock_all_path,path))return 1;
    memcpy(unlock_all_path,path,strlen(path)+1);
    unlock_all_loaded=1;unlock_all_value=0;
    FILE *file=unlock_all_open(path,"rb");
    if(file) {
        if(fgets(line,sizeof line,file) && !strcmp(line,"TSFP_UNLOCK_ALL_V1\n") &&
           fgets(line,sizeof line,file) && line[0]=='1' &&
           (line[1]=='\n'||line[1]=='\r'||line[1]==0)) unlock_all_value=1;
        fclose(file);
    }
    return 1;
}

static int unlock_all_save(int enabled)
{
    char temporary[sizeof unlock_all_path+8];
    if(snprintf(temporary,sizeof temporary,"%s.tmp",unlock_all_path)>=sizeof temporary)
        return 0;
    FILE *file=unlock_all_open(temporary,"wb");
    if(!file)return 0;
    int ok=fprintf(file,"TSFP_UNLOCK_ALL_V1\n%d\n",enabled)>0;
    if(fclose(file))ok=0;
    if(!ok) {
#ifdef _WIN32
        WCHAR wide[4210];
        if(MultiByteToWideChar(CP_UTF8,0,temporary,-1,wide,4210))DeleteFileW(wide);
#else
        remove(temporary);
#endif
        return 0;
    }
#ifdef _WIN32
    WCHAR from[4210],to[4210];
    if(!MultiByteToWideChar(CP_UTF8,0,temporary,-1,from,4210) ||
       !MultiByteToWideChar(CP_UTF8,0,unlock_all_path,-1,to,4210) ||
       !MoveFileExW(from,to,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(from);return 0;
    }
#else
    if(rename(temporary,unlock_all_path)) {remove(temporary);return 0;}
#endif
    return 1;
}

static void unlock_all_append(uint32_t menu)
{
    unlock_all_menu=menu;
    if(!unlock_all_sync())return;
    unlock_all_caption();
    if(unlock_all_text)
        guest_call(sub_00079D60_gen,6,unlock_all_text,UINT32_MAX,0u,0u,
                   UNLOCK_ALL_TOGGLE_CB,0u);
}

static void unlock_all_toggle(void)
{
    GuestRegisters r;save_registers(&r);
    if(unlock_all_sync() && unlock_all_save(!unlock_all_value)) {
        unlock_all_value=!unlock_all_value;
        unlock_all_caption();
        /* The original Cheats builder filters rows when it opens. Rebuild
           this page now so newly eligible cheats appear without backing out. */
        if(unlock_all_menu && guest_writable(unlock_all_menu,0x21c))
            guest_call(sub_00078980,1,unlock_all_menu);
    }
    restore_registers(&r);g_esp+=4;
}

void pc_display_unlock_all_check(void (*original)(void))
{
    original();
    if(!g_eax && unlock_all_sync() && unlock_all_value)g_eax=1;
}
