// Test-only parent. Exposes the trusted canary PID then waits to be killed,
// proving the production Process lifetime boundary without a worker watchdog.
#include "mod_process.hpp"
#include <cstdio>
#include <chrono>
#include <thread>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
static int run(const std::string &image,const std::string &cache) {
    std::string error;auto child=tsfp::mods::Process::start(image,cache,error);
    if(!child){fprintf(stderr,"%s\n",error.c_str());return 125;}
    const char *data="lifetime\n-\n0 0 0\n-\n";size_t length=strlen(data),sent=0;
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    std::string identity;
    while(std::chrono::steady_clock::now()<deadline) {
        if(sent<length){int n=child->write(data+sent,length-sent);if(n<0)return 125;sent+=(size_t)n;}
        char byte;int n=child->read(&byte,1);if(n<0)return 125;
        if(n){if(byte=='\n')break;identity+=byte;if(identity.size()>32)return 125;}
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    if(identity.empty())return 125;
    printf("%s\n",identity.c_str());fflush(stdout);
    (void)getchar();return 0;
}
#ifdef _WIN32
int wmain(int argc,wchar_t **argv) {
    if(argc!=3)return 2;
    auto utf8=[](const wchar_t *text){int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,nullptr,0,nullptr,nullptr);
        if(size<=0)return std::string();std::string result((size_t)size,0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,&result[0],size,nullptr,nullptr);result.pop_back();return result;};
    return run(utf8(argv[1]),utf8(argv[2]));
}
#else
int main(int argc,char **argv){return argc==3?run(argv[1],argv[2]):2;}
#endif
