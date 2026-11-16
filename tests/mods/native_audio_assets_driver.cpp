/* Actual immutable data loader, no guest/game/OS audio device needed. */
#include "mod_audio_assets.hpp"
#include "mod_audio_capsule.hpp"
#include "mod_files.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
static std::atomic<size_t> allocations{0};
void *operator new(size_t n) {allocations.fetch_add(1);if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void *p) noexcept {std::free(p);}
void operator delete(void *p,size_t) noexcept {std::free(p);}
static int run(int argc,char **argv) {
    try {
        using namespace tsfp::mods;
        if(argc<4)return 2;std::string mode=argv[1];size_t at=2;
        if(mode=="capsule" || mode=="capsule-cancel") {
            if(argc<6)return 2;std::vector<std::string> owners;
            for(int i=5;i<argc;++i)owners.push_back(argv[i]);
            std::string error;std::atomic<bool> cancelled{mode=="capsule-cancel"};
            auto store=read_audio_capsule(argv[2],argv[3],argv[4],owners,error,&cancelled);
            if(!store){std::cout<<"{\"accepted\":false,\"error_set\":"<<(!error.empty()?"true":"false")<<"}\n";return 0;}
            if(!error.empty())return 3;
            std::cout<<"{\"accepted\":true,\"count\":"<<store->count()<<",\"bytes\":"<<store->bytes()<<"}\n";return 0;
        }
        size_t count=std::stoul(argv[at++]);std::vector<std::string> packages;
        for(size_t i=0;i<count;++i){if(at>=(size_t)argc)return 2;packages.push_back(argv[at++]);}
        if(at>=(size_t)argc)return 2;count=std::stoul(argv[at++]);std::vector<AudioAssetPin> pins;
        for(size_t i=0;i<count;++i) {
            if((size_t)argc-at<5)return 2;AudioAssetPin pin;
            auto key=std::stoull(argv[at++]);if(key>UINT32_MAX)return 2;pin.key=(uint32_t)key;
            pin.package=argv[at++];pin.path=argv[at++];pin.sha256=argv[at++];pin.bytes=std::stoull(argv[at++]);
            pins.push_back(std::move(pin));
        }
        if(at!=(size_t)argc)return 2;
        std::atomic<bool> cancelled{mode=="cancel"};std::string error="stale";
        auto assets=AudioAssets::load(packages,pins,error,&cancelled);
        if(!assets){std::cout<<"{\"accepted\":false,\"error_set\":"<<(!error.empty()?"true":"false")<<"}\n";return 0;}
        if(!error.empty())return 3;
        if(mode=="snapshot"){std::cout<<"ready\n"<<std::flush;if(std::cin.get()!='x')return 4;}
        size_t before=allocations.load();
        for(unsigned i=0;i<1000;++i) {
            for(const auto &pin:pins)if(!assets->find(pin.key))return 5;
            if(assets->find(UINT32_MAX))return 6;
        }
        if(before!=allocations.load())return 7;
        std::cout<<"{\"accepted\":true,\"count\":"<<assets->count()<<",\"bytes\":"<<assets->bytes()<<",\"views\":[";
        bool first=true;
        for(const auto &pin:pins) {
            const auto *wave=assets->find(pin.key);if(!first)std::cout<<',';first=false;
            auto pcm=sha256_bytes(std::string(reinterpret_cast<const char *>(wave->pcm),wave->bytes));
            std::cout<<"{\"key\":"<<pin.key<<",\"rate\":"<<wave->rate<<",\"channels\":"<<wave->channels
                     <<",\"frames\":"<<wave->frames<<",\"pcm_sha256\":\""<<pcm<<"\"}";
        }
        std::cout<<"]}\n";return 0;
    }catch(...){return 8;}
}
#ifdef _WIN32
int wmain(int argc,wchar_t **argv) {
    std::vector<std::string> text((size_t)argc);std::vector<char *> args((size_t)argc);
    for(int i=0;i<argc;++i) {
        int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,nullptr,0,nullptr,nullptr);
        if(n<=0)return 2;text[(size_t)i].resize((size_t)n);
        if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,text[(size_t)i].data(),n,nullptr,nullptr))return 2;
        text[(size_t)i].pop_back();args[(size_t)i]=text[(size_t)i].data();
    }
    return run(argc,args.data());
}
#else
int main(int argc,char **argv){return run(argc,argv);}
#endif
