#include "mod_saves.hpp"
#include <cstdio>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
static int run(const std::string &mode,const std::string &originals,const std::string &profile) {
    std::string error;auto saves=tsfp::mods::SaveNamespace::open(originals,profile,error);
    printf("{\"accepted\":%s}\n",saves?"true":"false");fflush(stdout);
    if(!saves)return error.empty()?3:0;
    if(mode=="crash") {
#ifdef _WIN32
        TerminateProcess(GetCurrentProcess(),19);
#else
        _exit(19);
#endif
    }
    if(mode=="hold") {
        (void)getchar();bool checked=saves->recheck(error);
        printf("{\"rechecked\":%s}\n",checked?"true":"false");fflush(stdout);
    }
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
