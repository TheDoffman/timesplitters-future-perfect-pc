/* Activate future profile opens without remounting caches or partition devices.
   Existing profiles and open handles stay on their original drive. */
#include <errno.h>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif
static char storage_default[4096];
static int (*storage_profile_router)(const char *);
void pc_storage_set_profile_router(int (*route)(const char *))
{ storage_profile_router=route; }

static int storage_locked(void)
{ const char *s=getenv("TSFP_SAVE_LOCATION_LOCK");return s&&strcmp(s,"0"); }

static int storage_preference(char *out,size_t capacity)
{
    const char *input=getenv("RECOMP_INPUT_CONFIG");
    if(!input||!input[0])return 0;
    const char *slash=strrchr(input,'/'),*back=strrchr(input,'\\');
    if(back&&(!slash||back>slash))slash=back;
    size_t prefix=slash?(size_t)(slash-input)+1:0;
    const char name[]="save-location.txt";
    if(prefix+sizeof name>capacity)return 0;
    memcpy(out,input,prefix);memcpy(out+prefix,name,sizeof name);return 1;
}

static FILE *storage_fopen(const char *path,const char *mode)
{
#ifdef _WIN32
    WCHAR wide[4096],wmode[8];
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,4096)||
       !MultiByteToWideChar(CP_UTF8,0,mode,-1,wmode,8))return NULL;
    FILE *file=NULL;return _wfopen_s(&file,wide,wmode)?NULL:file;
#else
    return fopen(path,mode);
#endif
}

static int storage_directory(const char *path,int create)
{
#ifdef _WIN32
    WCHAR wide[4096];
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,4096))return 0;
    DWORD attr=GetFileAttributesW(wide);
    if(attr!=INVALID_FILE_ATTRIBUTES)return !!(attr&FILE_ATTRIBUTE_DIRECTORY);
    return create&&CreateDirectoryW(wide,NULL);
#else
    struct stat s;
    if(!stat(path,&s))return S_ISDIR(s.st_mode);
    return create&&!mkdir(path,0700);
#endif
}

static int storage_absolute(const char *path)
{
    if(!path[0]||strchr(path,'\n')||strchr(path,'\r'))return 0;
#ifdef _WIN32
    return (strlen(path)>2&&path[1]==':'&&(path[2]=='/'||path[2]=='\\'))||
           (path[0]=='\\'&&path[1]=='\\');
#else
    return path[0]=='/';
#endif
}

int pc_storage_resolve_root(const char *fallback,char *out,size_t capacity)
{
    char preference[4096],line[4096];
    snprintf(storage_default,sizeof storage_default,"%s",fallback?fallback:"");
    if(storage_locked()||!storage_preference(preference,sizeof preference))return 0;
    FILE *file=storage_fopen(preference,"rb");
    if(!file)return errno==ENOENT?0:-1;
    size_t n=fread(line,1,sizeof line-1,file);int bad=ferror(file)||!feof(file);
    fclose(file);line[n]=0;
    while(n&&(line[n-1]=='\n'||line[n-1]=='\r'))line[--n]=0;
    if(bad||memchr(line,0,n)||!storage_absolute(line)||n>=capacity||!storage_directory(line,0))return -1;
    memcpy(out,line,n+1);return 1;
}

#ifdef _WIN32
static int storage_volume(const char *path,WCHAR *volume,DWORD capacity)
{
    WCHAR wide[4096],resolved[4096];
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,4096))return 0;
    HANDLE file=CreateFileW(wide,0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                            NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
    if(file!=INVALID_HANDLE_VALUE) {
        DWORD n=GetFinalPathNameByHandleW(file,resolved,4096,VOLUME_NAME_GUID);
        if(n&&n<4096) {
            WCHAR *end=wcschr(resolved,L'}');
            if(end&&end[1]==L'\\') {end[2]=0;CloseHandle(file);return !wcscpy_s(volume,capacity,resolved);}
        }
        n=GetFinalPathNameByHandleW(file,resolved,4096,FILE_NAME_NORMALIZED);
        CloseHandle(file);
        if(n&&n<4096)wcscpy_s(wide,4096,resolved);
    }
    return GetVolumePathNameW(wide,volume,capacity)!=0;
}
#endif
static int storage_same_volume(const char *a,const char *b)
{
#ifdef _WIN32
    WCHAR ra[4096],rb[4096];
    return storage_volume(a,ra,4096)&&storage_volume(b,rb,4096)&&!_wcsicmp(ra,rb);
#elif defined(__APPLE__)
    struct statfs sa,sb;
    return !statfs(a,&sa)&&!statfs(b,&sb)&&!memcmp(&sa.f_fsid,&sb.f_fsid,sizeof sa.f_fsid);
#else
    return !strcmp(a,b);
#endif
}
static int storage_game_volume(const char *root)
{
    char executable[4096];
#ifdef _WIN32
    WCHAR wide[4096];DWORD n=GetModuleFileNameW(NULL,wide,4096);
    if(!n||n>=4096||!WideCharToMultiByte(CP_UTF8,0,wide,-1,executable,sizeof executable,NULL,NULL))return 0;
#elif defined(__APPLE__)
    uint32_t size=sizeof executable;
    if(_NSGetExecutablePath(executable,&size))return 0;
#else
    return 0;
#endif
    return storage_same_volume(root,executable);
}

static void storage_drive(PcStorageDrive *d,const char *root,const char *label,const char *suffix)
{
    memset(d,0,sizeof *d);snprintf(d->path,sizeof d->path,"%s",root);
    /* Never interpret volume labels as native text-format controls. */
    char safe[25];size_t n=0;
    for(size_t i=0;label[i]&&n<sizeof safe-1;i++)if((unsigned char)label[i]>=32&&(unsigned char)label[i]<127)safe[n++]=label[i];
    safe[n]=0;
    snprintf(d->name,sizeof d->name,"%s%s%s",n?safe:"Local Disk",suffix[0]?" ":"",suffix);
    d->current=storage_same_volume(root,storage_path);
    d->game=storage_game_volume(root);
    if(d->current)snprintf(d->target,sizeof d->target,"%s",storage_path);
    else if(storage_default[0]&&storage_same_volume(root,storage_default))snprintf(d->target,sizeof d->target,"%s",storage_default);
    else snprintf(d->target,sizeof d->target,"%s%sTimeSplittersFuturePerfect/Saves",root,
                  root[strlen(root)-1]=='/'||root[strlen(root)-1]=='\\'?"":"/");
#ifdef _WIN32
    WCHAR wide[4096];ULARGE_INTEGER available,total,free_bytes;
    if(MultiByteToWideChar(CP_UTF8,0,root,-1,wide,4096)&&GetDiskFreeSpaceExW(wide,&available,&total,&free_bytes)) {
        d->available=available.QuadPart;d->total=total.QuadPart;d->known=1;
    }
#elif defined(__APPLE__)
    struct statfs disk;
    if(!statfs(root,&disk)){d->available=(uint64_t)disk.f_bavail*disk.f_bsize;d->total=(uint64_t)disk.f_blocks*disk.f_bsize;d->known=1;}
#endif
}

int pc_storage_drives(PcStorageDrive *out,int capacity)
{
    int count=0;
    if(!out||capacity<=0)return 0;
#ifdef _WIN32
    WCHAR roots[512];DWORD n=GetLogicalDriveStringsW(512,roots);
    if(!n||n>=512)return 0;
    for(const WCHAR *root=roots;*root&&count<capacity;root+=wcslen(root)+1) {
        WCHAR label[128]=L"";char utf8[512]="",path[32],suffix[8];
        UINT kind=GetDriveTypeW(root);
        if(kind==DRIVE_NO_ROOT_DIR||kind==DRIVE_UNKNOWN)continue;
        GetVolumeInformationW(root,label,128,NULL,NULL,NULL,NULL,0);
        WideCharToMultiByte(CP_UTF8,0,label,-1,utf8,sizeof utf8,NULL,NULL);
        if(!utf8[0])snprintf(utf8,sizeof utf8,"%s",kind==DRIVE_REMOTE?"Network Drive":kind==DRIVE_REMOVABLE?"Removable Disk":kind==DRIVE_CDROM?"Optical Drive":"Local Disk");
        snprintf(path,sizeof path,"%c:/",(char)root[0]);snprintf(suffix,sizeof suffix,"(%c:)",(char)root[0]);
        storage_drive(&out[count++],path,utf8,suffix);
    }
#elif defined(__APPLE__)
    struct statfs *mounts;int n=getmntinfo(&mounts,MNT_NOWAIT);
    for(int i=0;i<n&&count<capacity;i++) {
        const char *root=mounts[i].f_mntonname;
        if(strcmp(root,"/")&&strncmp(root,"/Volumes/",9))continue;
        struct attrlist attrs={0};
        struct {uint32_t length;attrreference_t name;char text[1024];} result={0};
        const char *name=strrchr(root,'/');name=name&&name[1]?name+1:"Macintosh HD";
        attrs.bitmapcount=ATTR_BIT_MAP_COUNT;attrs.volattr=ATTR_VOL_INFO|ATTR_VOL_NAME;
        if(!getattrlist(root,&attrs,&result,sizeof result,0)) {
            const char *p=(const char *)&result.name+result.name.attr_dataoffset;
            const char *end=(const char *)&result+sizeof result;
            if(p>=(const char *)&result&&p<end&&memchr(p,0,(size_t)(end-p)))name=p;
        }
        storage_drive(&out[count++],root,name,"");
    }
#endif
    return count;
}

/* Atomically replace only our own preference; never change controls or saves. */
static int storage_write_preference(const char *path,const char *text)
{
#ifdef _WIN32
    WCHAR destination[4096],temporary[4096];DWORD written;
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,destination,4096)||
       swprintf_s(temporary,4096,L"%s.%lu.tmp",destination,GetCurrentProcessId())<0)return 0;
    HANDLE file=CreateFileW(temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return 0;
    DWORD length=(DWORD)strlen(text);
    int ok=WriteFile(file,text,length,&written,NULL)&&written==length&&FlushFileBuffers(file);
    if(!CloseHandle(file))ok=0;
    if(ok)ok=MoveFileExW(temporary,destination,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileW(temporary);return ok;
#else
    char temporary[4096];
    if(snprintf(temporary,sizeof temporary,"%s.%ld.tmp",path,(long)getpid())>=(int)sizeof temporary)return 0;
    int fd=open(temporary,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0)return 0;
    size_t length=strlen(text),at=0;int ok=1;
    while(at<length){ssize_t n=write(fd,text+at,length-at);if(n<0&&errno==EINTR)continue;if(n<=0){ok=0;break;}at+=(size_t)n;}
    if(fsync(fd))ok=0;if(close(fd))ok=0;
    if(ok)ok=rename(temporary,path)==0;if(!ok)unlink(temporary);return ok;
#endif
}

int pc_storage_choose(const PcStorageDrive *drive,char *error,size_t capacity)
{
    char preference[4096],parent[4096];
    const char *failure="Could not save the drive preference.";
    if(storage_locked()){failure="Test session: save location is locked.";goto failed;}
    if(!drive||!storage_absolute(drive->target)||!storage_preference(preference,sizeof preference))goto failed;
    if(!storage_directory(drive->path,0)){failure="That drive is no longer available.";goto failed;}
    snprintf(parent,sizeof parent,"%s",drive->target);
    char *slash=strrchr(parent,'/'),*back=strrchr(parent,'\\');if(back&&(!slash||back>slash))slash=back;
    if(!slash)goto failed;*slash=0;
    if(!storage_directory(parent,1)||!storage_directory(drive->target,1)){
        failure="Cannot create saves on that drive.";goto failed;
    }
    /* XAPI asks for free space on U:\\ before creating a save. Its title
       directory must exist on a newly selected drive, just as at startup. */
    if(snprintf(parent,sizeof parent,"%s/UserData",drive->target)>=(int)sizeof parent ||
       !storage_directory(parent,1) ||
       snprintf(parent,sizeof parent,"%s/UserData/45410066",drive->target)>=(int)sizeof parent ||
       !storage_directory(parent,1)) {
        failure="Cannot create the profile folder.";goto failed;
    }
    /* Opening an exclusive throwaway file proves write permission without
       replacing an existing profile, even on a read-only mount. */
    char probe[4096];
    if(snprintf(probe,sizeof probe,"%s/.tsfp-write-check-%llu",drive->target,(unsigned long long)GetTickCount64())>=(int)sizeof probe)goto failed;
    FILE *file=storage_fopen(probe,"wbx");
    if(!file){failure="The save folder is not writable.";goto failed;}
    int ok=fclose(file)==0;
#ifdef _WIN32
    WCHAR wide[4096];if(MultiByteToWideChar(CP_UTF8,0,probe,-1,wide,4096))DeleteFileW(wide);
#else
    unlink(probe);
#endif
    if(!ok)goto failed;
    char previous[4096];
    AcquireSRWLockExclusive(&storage_lock);
    snprintf(previous,sizeof previous,"%s",storage_path);
    ReleaseSRWLockExclusive(&storage_lock);
    if(!storage_profile_router||!storage_profile_router(drive->target)) {
        failure="Could not activate that save folder.";goto failed;
    }
    if(!storage_write_preference(preference,drive->target)) {
        storage_profile_router(previous);goto failed;
    }
    pc_storage_set_path(drive->target);
    if(capacity)error[0]=0;return 1;
failed:
    if(capacity)snprintf(error,capacity,"%s",failure);return 0;
}
