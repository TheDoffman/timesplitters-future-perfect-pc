#include "mod_capsule.hpp"
#include "mod_files.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif
static int run(const std::string &mode,const std::string &path,const std::string &hash) {
    std::string error;auto capsule=tsfp::mods::SourceCapsule::read(path,hash,error);
    if(!capsule){puts("{\"accepted\":false}");return error.empty()?3:0;}
    auto source_hash=tsfp::mods::sha256_bytes(capsule->source());
    if(mode=="snapshot") {std::ofstream changed(std::filesystem::u8path(path),std::ios::binary|std::ios::trunc);changed<<"replaced after capture";}
    printf("{\"accepted\":true,\"bytes\":%zu,\"source_sha256\":\"%s\",\"profile\":\"%s\",\"package\":\"%s\",\"unchanged\":%s}\n",
        capsule->source().size(),source_hash.c_str(),capsule->profile().c_str(),capsule->package().c_str(),
        source_hash==tsfp::mods::sha256_bytes(capsule->source())?"true":"false");return 0;
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
