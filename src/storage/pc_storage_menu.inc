/* Host save locations are rows in the original Create Profile storage dialog.
   Never pass host row IDs into the console device table. The native profile
   state machine still owns Back, device validation and name entry. */
#include "pc_storage.h"
#define SD_SELECT 0x12379u
#define SD_DRAW 0x1237au
extern void sub_00192E60(void),sub_00192770(void),sub_00191470(void);
extern void sub_00024EC0(void);
static uint32_t sd_storage;
static PcStorageDrive sd_drives[PC_STORAGE_DRIVES];
static int sd_count,sd_selected;
static ULONGLONG sd_notice_until;
static char sd_notice[160];
static uint32_t sd_picker_menu;
#define SD_TEXT(i) (sd_storage+(uint32_t)(i)*256u)
#define SD_DETAILS PC_STORAGE_DRIVES
static int sd_allocate(void)
{
    if(!sd_storage)sd_storage=xbox_HeapAlloc((PC_STORAGE_DRIVES+1)*256,4);
    return sd_storage!=0;
}
static uint32_t sd_text(unsigned i,const char *format,...)
{
    va_list args;va_start(args,format);
    vsnprintf((char *)((uintptr_t)SD_TEXT(i)+g_xbox_mem_offset),256,format,args);
    va_end(args);return SD_TEXT(i);
}
static void sd_details(void)
{
    if(!sd_storage)return;
    if(sd_notice[0]){sd_text(SD_DETAILS,"%s",sd_notice);return;}
    if(sd_selected<0||sd_selected>=sd_count){sd_text(SD_DETAILS,"Space unavailable");return;}
    PcStorageDrive *d=&sd_drives[sd_selected];char free_size[32],total[32],path[72];
    size_t length=strlen(d->target);
    if(length>64)snprintf(path,sizeof path,"%.28s...%s",d->target,d->target+length-32);
    else snprintf(path,sizeof path,"%s",d->target);
    pc_storage_size(d->available,free_size,sizeof free_size);
    pc_storage_size(d->total,total,sizeof total);
    if(sd_selected==2)sd_text(SD_DETAILS,"Select a folder. Existing saves are kept.\nCopy saves using the launcher if needed.");
    else sd_text(SD_DETAILS,"\vs60%s\vs100\n%s free / %s total",path,free_size,total);
}
/* Called only at the native storage row construction site (001944C3).
   Duplicate the original ABI with host labels and host callbacks. */
static void sd_buttons(void (*original)(void))
{
    GuestRegisters before,after;save_registers(&before);
    uint32_t frame=g_esp,args[6];
    for(unsigned i=0;i<6;i++)args[i]=WORD_AT(frame+4+i*4);
    if(args[1] || !sd_allocate()){original();return;}
    sd_count=pc_storage_locations(sd_drives,PC_STORAGE_DRIVES);
    if(!sd_count){original();return;}
    sd_selected=0;sd_notice[0]=0;sd_details();
    for(int i=0;i<sd_count;i++) {
        restore_registers(&before);
        WORD_AT(frame+4)=sd_text((unsigned)i,"\vs85%s\vs100",sd_drives[i].name);
        WORD_AT(frame+8)=(uint32_t)i;
        WORD_AT(frame+20)=SD_SELECT;WORD_AT(frame+24)=SD_DRAW;
        original();if(!i)save_registers(&after);
    }
    for(unsigned i=0;i<6;i++)WORD_AT(frame+4+i*4)=args[i];
    restore_registers(&after);
}
static void sd_capacity(void (*original)(void))
{
    uint32_t frame=g_esp,old=WORD_AT(frame+4);
    if(sd_storage&&sd_count)WORD_AT(frame+4)=SD_TEXT(SD_DETAILS);
    original();WORD_AT(frame+4)=old;
}
static void sd_select(void)
{
    GuestRegisters r;save_registers(&r);
    uint32_t menu=WORD_AT(g_esp+4);
    int id=guest_writable(menu,0x190)?(int)WORD_AT(menu+0x18c):-1;
    if(id>=0&&id<sd_count) {
        sd_selected=id;
        if(id==2) {
            if(!sd_picker_menu && pc_storage_picker_start()){sd_picker_menu=menu;fprintf(stderr,"[SAVE-FOLDER] picker opened for menu %08x\n",menu);snprintf(sd_notice,sizeof sd_notice,"Choose a save folder in the desktop window.");sd_notice_until=GetTickCount64()+3600000;}
            restore_registers(&r);g_esp+=4;return;
        }
        if(!sd_drives[id].current) {
            if(!pc_storage_choose(&sd_drives[id],sd_notice,sizeof sd_notice)) {
                sd_notice_until=GetTickCount64()+5000;sd_details();
                restore_registers(&r);g_esp+=4;return;
            }
            /* Same synchronous profile scan used by native 00193DF2. The
               duplicate-name check must inspect the newly selected drive. */
            WORD_AT(0x74f9fc)=0;
            guest_call(sub_00024EC0,3,0xffffffffu,0x190f70u,0u);
            WORD_AT(0x74e938)=1;
        }
        for(int i=0;i<sd_count;i++)sd_drives[i].current=i==id;
        WORD_AT(menu+0x18c)=0;WORD_AT(0x7a1564)=0;
        guest_call(sub_00192E60,1,menu);
    }
    restore_registers(&r);g_esp+=4;
}
static void sd_draw(void)
{
    /* Keep the native drive icon, but its console lookup always uses device 0. */
    uint32_t row=WORD_AT(g_esp+8);
    if(!guest_writable(row,0x18)){g_esp+=4;return;}
    uint32_t id=WORD_AT(row+0x14);WORD_AT(row+0x14)=0;
    sub_00191470();WORD_AT(row+0x14)=id;
}
static void sd_profile_tick(void)
{
    uint32_t menu=WORD_AT(g_esp+4),player=guest_writable(menu,0x190)?WORD_AT(menu+0x38):4;
    int active=player<4&&WORD_AT(0x74f9ec+player*4)==6;
    /* Other profile menus also tick. They must not drain this owner's result. */
    if(sd_picker_menu && sd_picker_menu==menu) {
        char chosen[4096];int result=pc_storage_picker_poll(chosen,sizeof chosen);
        if(result) {
            uint32_t pending=sd_picker_menu;sd_picker_menu=0;sd_notice[0]=0;
            fprintf(stderr,"[SAVE-FOLDER] picker result %d, active storage menu %d\n",result,active);
            if(result>0 && active && pending==menu) {
                if(pc_storage_folder(chosen,sd_notice,sizeof sd_notice)) {
                    fprintf(stderr,"[SAVE-FOLDER] folder activated; continuing profile creation\n");
                    WORD_AT(0x74f9fc)=0;guest_call(sub_00024EC0,3,0xffffffffu,0x190f70u,0u);WORD_AT(0x74e938)=1;
                    WORD_AT(menu+0x18c)=0;WORD_AT(0x7a1564)=0;guest_call(sub_00192E60,1,menu);
                } else {fprintf(stderr,"[SAVE-FOLDER] %s\n",sd_notice);sd_notice_until=GetTickCount64()+10000;}
            }
        }
    }
    if(active) {
        int selected=(int)WORD_AT(menu+0x18c);
        sd_selected=selected;
        if(GetTickCount64()>=sd_notice_until)sd_notice[0]=0;
        sd_details();WORD_AT(0x7a1564)=0;
    }
    sub_00192770();
    /* This native update copies its highlighted row to the console index.
       Host rows must never escape into capacity/device-array consumers. */
    if(active)WORD_AT(0x7a1564)=0;
}

static void sd_width(void)
{
    /* Native sub_00075B70 stores percentage width in the builder and menu.
       Set it before the storage title/rows, leaving other profile pages alone. */
    uint32_t builder=WORD_AT(0x7bb1e0),menu=WORD_AT(0x7bb1e8);
    if(guest_writable(builder,0x38)&&guest_writable(menu,0x5c)) {
        WORD_AT(builder+0x34)=bits(65.f);
        WORD_AT(menu+0x50)=WORD_AT(menu+0x54)=WORD_AT(menu+0x58)=bits(65.f);
    }
}
