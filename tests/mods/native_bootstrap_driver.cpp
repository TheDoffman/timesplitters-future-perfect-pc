#include "mod_bootstrap.h"
extern "C" {
#include "../runtime/guest_storage.h"
}
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif
static tsfp_storage_state storage;
static std::string selected;
static bool locked=false,wrong=false,late=false;
extern "C" void pc_storage_lock_locations(void){locked=true;}
extern "C" int pc_storage_resolve_root(const char *,char *out,size_t capacity) {
    if(locked)return -1;
    if(selected=="fail")return -1;
    if(selected.empty())return 0;
    if(selected.size()>=capacity)return -1;
    std::memcpy(out,selected.c_str(),selected.size()+1);return 1;
}
extern "C" int xbox_require_save_root_utf8(const char *path) {
    return tsfp_storage_require(&storage,path);
}
extern "C" int xbox_save_root_utf8(char *out,unsigned capacity) {
    tsfp_storage_snapshot snapshot;
    if(!tsfp_storage_read(&storage,&snapshot))return 0;
    const char *root=wrong?"wrong-root":snapshot.save;
    if(std::strlen(root)>=capacity)return 0;
    std::strcpy(out,root);return 1;
}
static int run(const std::string &mode,const std::string &original,const std::string &preference) {
    selected=preference=="-"?"":preference;wrong=mode=="wrong-root";late=mode=="late";
    tsfp_storage_snapshot paths={};std::strcpy(paths.game,"/fixture-game");
    if(original.size()>=sizeof paths.save)return 2;
    std::strcpy(paths.save,original.c_str());
    if(late && !tsfp_storage_commit(&storage,&paths))return 3;
    bool before=pc_mod_bootstrap_source(nullptr)!=nullptr;
    int prepared=pc_mod_bootstrap_prepare((mode=="no-root" || mode=="environment-root")?nullptr:original.c_str());
    size_t length=99;bool premature=pc_mod_bootstrap_source(&length)!=nullptr || length!=0;
    char required[4096]={};int fence=tsfp_storage_required(&storage,required,sizeof required);
    bool escape=false;int committed=0,rechecked=0;
    if(prepared) {
        if(fence==1) {
            escape=tsfp_storage_commit(&storage,&paths)!=0;
            std::strcpy(paths.save,required);
        }
        if(mode!="no-storage")tsfp_storage_commit(&storage,&paths);
        committed=pc_mod_bootstrap_commit();
        if(committed && mode=="hold") {std::puts("{\"holding\":true}");std::fflush(stdout);(void)std::getchar();}
        if(committed && mode=="repeat")pc_mod_bootstrap_prepare(original.c_str());
        rechecked=pc_mod_bootstrap_recheck();
    }
    const char *source=pc_mod_bootstrap_source(&length);
    const char *profile=pc_mod_bootstrap_profile();
    std::printf("{\"prepared\":%s,\"committed\":%s,\"ready\":%s,\"rechecked\":%s,\"locked\":%s,\"fence\":%d,\"escape\":%s,\"premature\":%s,\"bytes\":%zu,\"profile\":\"%s\"}\n",
        prepared?"true":"false",committed?"true":"false",source?"true":"false",rechecked?"true":"false",locked?"true":"false",fence,escape?"true":"false",(before||premature)?"true":"false",length,profile?profile:"");
    return 0;
}
#ifdef _WIN32
int wmain(int argc,wchar_t **argv) {
    if(argc!=4)return 2;std::string args[3];
    for(int i=0;i<3;++i){int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i+1],-1,nullptr,0,nullptr,nullptr);if(n<=0)return 2;
        args[i].resize((size_t)n);if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i+1],-1,args[i].data(),n,nullptr,nullptr))return 2;args[i].pop_back();}
    return run(args[0],args[1],args[2]);
}
#else
int main(int argc,char **argv){return argc==4?run(argv[1],argv[2],argv[3]):2;}
#endif
