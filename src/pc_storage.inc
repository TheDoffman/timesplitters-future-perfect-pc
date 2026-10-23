/* Included by pc_prompts.c on both hosts. Probe only while storage text is
   requested, at most once every five seconds (including failed probes). */
#include "pc_storage.h"
#ifdef __APPLE__
#include <sys/mount.h>
#include <sys/attr.h>
#include <unistd.h>
#include <limits.h>
#endif

static SRWLOCK storage_lock=SRWLOCK_INIT;
static char storage_path[4096];
static PcStorageInfo storage_info;
static uint64_t storage_deadline;

void pc_storage_set_path(const char *path)
{
    AcquireSRWLockExclusive(&storage_lock);
    snprintf(storage_path,sizeof storage_path,"%s",path?path:"");
    memset(&storage_info,0,sizeof storage_info);
    storage_deadline=0;
    ReleaseSRWLockExclusive(&storage_lock);
}

/* The original font lacks arbitrary Unicode. Keep unrenderable characters
   out of formatting controls and use the drive identifier as a fallback. */
static void pc_storage_label(const char *label, const char *suffix)
{
    char safe[15];size_t n=0;
    for(size_t i=0;label[i]&&n<sizeof safe-1;++i) {
        unsigned char c=(unsigned char)label[i];
        if(c>=32&&c<127)safe[n++]=(char)c;
    }
    safe[n]=0;
    snprintf(storage_info.label,sizeof storage_info.label,"%s%s%s",
             n?safe:"Local Disk",suffix[0]?" ":"",suffix);
}

static void pc_storage_probe(void)
{
    memset(&storage_info,0,sizeof storage_info);
    if(!storage_path[0])return;
#ifdef _WIN32
    WCHAR path[4096],resolved[4096],volume[4096],label[128];
    ULARGE_INTEGER available,total,free_bytes;
    char utf8[512],suffix[16]="";
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,storage_path,-1,path,4096))return;
    /* Resolve junctions/symlinks so redirected saves report their real volume. */
    HANDLE dir=CreateFileW(path,0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                          NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
    if(dir!=INVALID_HANDLE_VALUE) {
        DWORD count=GetFinalPathNameByHandleW(dir,resolved,4096,FILE_NAME_NORMALIZED);
        CloseHandle(dir);
        if(count&&count<4096)wcscpy_s(path,4096,resolved);
    }
    if(GetVolumePathNameW(path,volume,4096)) {
        const WCHAR *drive=volume;
        if(!wcsncmp(drive,L"\\\\?\\",4))drive+=4;
        if(drive[0]&&drive[1]==L':')snprintf(suffix,sizeof suffix,"(%c:)",(char)drive[0]);
        if(GetVolumeInformationW(volume,label,128,NULL,NULL,NULL,NULL,0)&&
           WideCharToMultiByte(CP_UTF8,0,label,-1,utf8,sizeof utf8,NULL,NULL))
            pc_storage_label(utf8,suffix);
        else pc_storage_label("Local Disk",suffix);
    }
    if(GetDiskFreeSpaceExW(path,&available,&total,&free_bytes)) {
        storage_info.available=available.QuadPart;
        storage_info.total=total.QuadPart;
        storage_info.known=1;
    }
#elif defined(__APPLE__)
    struct statfs disk;
    char resolved[PATH_MAX];
    const char *path=realpath(storage_path,resolved)?resolved:storage_path;
    if(!statfs(path,&disk)) {
        struct attrlist attrs={0};
        struct {uint32_t length;attrreference_t name;char text[1024];} result={0};
        attrs.bitmapcount=ATTR_BIT_MAP_COUNT;
        attrs.volattr=ATTR_VOL_INFO|ATTR_VOL_NAME;
        if(!getattrlist(path,&attrs,&result,sizeof result,0)) {
            const char *name=(const char *)&result.name+result.name.attr_dataoffset;
            const char *end=(const char *)&result+sizeof result;
            if(name>=(const char *)&result&&name<end&&memchr(name,0,(size_t)(end-name)))
                pc_storage_label(name,"");
        }
        storage_info.available=(uint64_t)disk.f_bavail*(uint64_t)disk.f_bsize;
        storage_info.total=(uint64_t)disk.f_blocks*(uint64_t)disk.f_bsize;
        storage_info.known=1;
    }
#endif
}

static int pc_storage_rewrite(const char *source,char *out,size_t capacity)
{
    int result;
    if(!strstr(source,"Xbox Hard Disk")&&!pc_storage_capacity_line(source))return 0;
    AcquireSRWLockExclusive(&storage_lock);
    uint64_t now=GetTickCount64();
    if(!storage_deadline||now>=storage_deadline) {
        pc_storage_probe();storage_deadline=now+5000;
    }
    result=pc_storage_format(source,&storage_info,out,capacity);
    ReleaseSRWLockExclusive(&storage_lock);
    return result;
}
#include "pc_storage_routes.inc"
