#ifndef TSFP_PC_STORAGE_H
#define TSFP_PC_STORAGE_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Presentation only: never changes the guest disk geometry or save routing. */
typedef struct PcStorageInfo {
    char label[64];
    uint64_t available, total;
    int known;
} PcStorageInfo;

void pc_storage_set_path(const char *path);
/* Register the kernel's UDATA-only routing adapter during host startup. */
void pc_storage_set_profile_router(int (*route)(const char *));
#define PC_STORAGE_DRIVES 32
typedef struct PcStorageDrive {
    char name[96], path[4096], target[4096];
    uint64_t available,total;
    int current,game,known;
} PcStorageDrive;
/* Nonblocking host picker: 0 pending, 1 selected, -1 cancelled/failed. */
int pc_storage_picker_start(void);
int pc_storage_picker_poll(char *out,size_t capacity);
int pc_storage_locations(PcStorageDrive *out,int capacity);
int pc_storage_folder(const char *path,char *error,size_t capacity);
int pc_storage_drives(PcStorageDrive *out,int capacity);
int pc_storage_choose(const PcStorageDrive *drive,char *error,size_t capacity);
/* -1: configured drive is unavailable; 0: default route; 1: explicit route. */
int pc_storage_resolve_root(const char *fallback,char *out,size_t capacity);

static inline int pc_storage_blocks(const char **cursor)
{
    const char *p=*cursor;
    if(*p<'0'||*p>'9')return 0;
    while(*p>='0'&&*p<='9')++p;
    if(*p=='+')++p;
    *cursor=p;
    return 1;
}

static inline int pc_storage_capacity_line(const char *text)
{
    const char *p=text;
    if(!pc_storage_blocks(&p)||strncmp(p," Free ",6))return 0;
    p+=6;
    return pc_storage_blocks(&p)&&!strcmp(p," Total");
}

static inline void pc_storage_size(uint64_t bytes, char *out, size_t size)
{
    static const char *units[]={"B","KiB","MiB","GiB","TiB","PiB"};
    double value=(double)bytes;
    unsigned unit=0;
    while(value>=1024.0&&unit<5){value/=1024.0;++unit;}
    snprintf(out,size,unit?"%.1f %s":"%.0f %s",value,units[unit]);
}

static inline int pc_storage_format(const char *source, const PcStorageInfo *info,
                             char *out, size_t capacity)
{
    char result[160],available[32],total[32];
    const char *old=strstr(source,"Xbox Hard Disk");
    if(old) {
        size_t prefix=(size_t)(old-source);const char *label=info->label[0]?info->label:"Save storage";
        size_t needed=prefix+strlen(label)+strlen(old+14);
        if(needed>=sizeof result || needed>=capacity)return 0;
        memcpy(result,source,prefix);strcpy(result+prefix,label);strcat(result,old+14);
    }
    else if(pc_storage_capacity_line(source)) {
        if(info->known) {
            pc_storage_size(info->available,available,sizeof available);
            pc_storage_size(info->total,total,sizeof total);
            snprintf(result,sizeof result,"%s free / %s",available,total);
        } else snprintf(result,sizeof result,"Space unavailable");
    } else return 0;
    if(strlen(result)>=capacity)return 0;
    memcpy(out,result,strlen(result)+1);
    return 1;
}
#endif
