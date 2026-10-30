/* Original translation adapters, selected by guest_kernel_storage.c after its
 * atomic root snapshot/native mkdir helpers. Console policy stays shared. */
#include "guest_paths.h"
#include "guest_storage.h"
#include "guest_kernel_symlinks.h"
static int bridge_path_link(void *context,const char *name,size_t n,char *out,size_t capacity,size_t *bytes) {
    (void)context;return tsfp_kernel_symlink_copy(name,n,out,capacity,bytes)==TSFP_SYMLINK_OK;
}
#ifdef _WIN32
static __declspec(thread) WCHAR bridge_path_last[MAX_PATH];
const wchar_t *xbox_LastHostPath(void) {return bridge_path_last;}
static int bridge_path_wide_join(const WCHAR *base,const WCHAR *relative,size_t m,WCHAR *out,size_t capacity) {
    size_t n=wcslen(base),floor=0,add,total;
    if(n && (base[0]=='/' || base[0]=='\\'))floor=1;
    if(n>=2 && base[0]=='\\' && base[1]=='\\')floor=2;
    if(n>=3 && base[1]==':' && (base[2]=='/' || base[2]=='\\'))floor=3;
    while(n>floor && (base[n-1]=='/' || base[n-1]=='\\'))--n;
    add=n && m && base[n-1]!='/' && base[n-1]!='\\';total=n+m+add;
    if(total>=capacity)return 0;
    memcpy(out,base,n*sizeof(WCHAR));if(add)out[n++]=L'\\';memcpy(out+n,relative,m*sizeof(WCHAR));out[total]=0;return 1;
}
#endif
BOOL xbox_translate_path(const char *input,xbox_host_char *output,DWORD capacity) {
    const tsfp_path_ops ops={NULL,bridge_path_link};tsfp_path_plan plan;
    char relative[TSFP_PATH_LIMIT];size_t n=0;tsfp_storage_snapshot snapshot;
#ifdef _WIN32
    WCHAR converted[MAX_PATH],candidate[MAX_PATH],directory[MAX_PATH],native_base[MAX_PATH];const WCHAR *base;
#else
    char candidate[MAX_PATH],directory[MAX_PATH];const char *base;size_t written;
#endif
    if(!input || !output || !capacity)return FALSE;
    while(n<TSFP_PATH_LIMIT && input[n])++n;
    if(n==TSFP_PATH_LIMIT)return FALSE;
    if(!bridge_path_snapshot(&snapshot)) {xbox_path_init(NULL,NULL);if(!bridge_path_snapshot(&snapshot))return FALSE;}
#ifdef _WIN32
    if(!tsfp_path_prepare(input,n,recomp_config_title_id(),'\\',&ops,relative,sizeof(relative),&plan))return FALSE;
    if(!MultiByteToWideChar(CP_ACP,0,relative,-1,converted,MAX_PATH))return FALSE;
    {const char *root=plan.root==TSFP_PATH_GAME?snapshot.game:plan.root==TSFP_PATH_PROFILE && snapshot.profile[0]?snapshot.profile:snapshot.save;
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,root,-1,native_base,MAX_PATH))return FALSE;base=native_base;}
    if(plan.root==TSFP_PATH_PASSTHROUGH) {
        n=wcslen(converted);if(n>=capacity)return FALSE;memcpy(candidate,converted,(n+1)*sizeof(WCHAR));
    } else if(!bridge_path_wide_join(base,converted,wcslen(converted),candidate,sizeof(candidate)/sizeof(candidate[0])) || wcslen(candidate)>=capacity)return FALSE;
    if(plan.directory_bytes && bridge_path_wide_join(base,converted,plan.directory_bytes,directory,sizeof(directory)/sizeof(directory[0])))
        SHCreateDirectoryExW(NULL,directory,NULL);
    memcpy(output,candidate,(wcslen(candidate)+1)*sizeof(WCHAR));
    memcpy(bridge_path_last,candidate,(wcslen(candidate)+1)*sizeof(WCHAR));
#else
    if(!tsfp_path_prepare(input,n,recomp_config_title_id(),'/',&ops,relative,sizeof(relative),&plan))return FALSE;
    base=plan.root==TSFP_PATH_GAME?snapshot.game:plan.root==TSFP_PATH_PROFILE && snapshot.profile[0]?snapshot.profile:snapshot.save;
    if(plan.root==TSFP_PATH_PASSTHROUGH) {
        if(plan.relative_bytes>=sizeof(candidate) || plan.relative_bytes>=capacity)return FALSE;
        memcpy(candidate,relative,plan.relative_bytes+1);
    } else if(!tsfp_path_join(base,strlen(base),relative,plan.relative_bytes,'/',candidate,sizeof(candidate),&written) || written>=capacity)return FALSE;
    if(plan.directory_bytes && tsfp_path_join(base,strlen(base),relative,plan.directory_bytes,'/',directory,sizeof(directory),&written))mkdir_p(directory);
    memcpy(output,candidate,strlen(candidate)+1);
#endif
    if(plan.root!=TSFP_PATH_PASSTHROUGH && plan.category!=TSFP_PATH_PARTITION)xbox_watch_note_path(input);
    return TRUE;
}
