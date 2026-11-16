#include "mod_audio_capsule.hpp"
#include "mod_files.hpp"
#include <algorithm>
#include <stdexcept>
namespace tsfp { namespace mods {
namespace {
void need(bool value){if(!value)throw std::runtime_error("Invalid audio capsule");}
uint64_t number(const std::string &bytes,size_t at,unsigned width) {
    uint64_t result=0;for(unsigned i=0;i<width;++i)result|=(uint64_t)(unsigned char)bytes[at+i]<<(8*i);return result;
}
std::string digest(const std::string &bytes,size_t at) {
    const char *hex="0123456789abcdef";std::string result(64,'0');
    for(size_t i=0;i<32;++i){auto value=(unsigned char)bytes[at+i];result[i*2]=hex[value>>4];result[i*2+1]=hex[value&15];}return result;
}
}
std::unique_ptr<const AudioAssets> read_audio_capsule(const std::string &path,
    const std::string &hash,const std::string &cache_root,
    const std::vector<std::string> &authorized,std::string &error,const std::atomic<bool> *cancel) {
    error.clear();
    try {
        need(!authorized.empty() && authorized.size()<=32);
        auto packages=authorized;for(const auto &package:packages)need(sha256_text(package));
        std::sort(packages.begin(),packages.end());
        need(std::adjacent_find(packages.begin(),packages.end())==packages.end());
        // Validate the root independently: a wire record can never choose it.
        auto root=checked_path(cache_root,true);
        constexpr size_t limit=16+32*32+AudioAssets::asset_limit*48;
        auto bytes=read_pinned_data_file(path,hash,limit,cancel);
        need(bytes.size()>=16 && bytes.compare(0,8,"TSFPAUD1")==0);
        auto owners=number(bytes,8,4),count=number(bytes,12,4);
        need(owners==packages.size() && count>0 && count<=AudioAssets::asset_limit &&
             bytes.size()==16+owners*32+count*48);
        for(size_t i=0;i<packages.size();++i)need(digest(bytes,16+i*32)==packages[i]);
        std::vector<AudioAssetPin> pins;pins.reserve((size_t)count);
        for(size_t i=0;i<count;++i) {
            size_t at=16+packages.size()*32+i*48;AudioAssetPin pin;
            pin.key=(uint32_t)number(bytes,at,4);auto owner=number(bytes,at+4,4);
            need(owner<packages.size() && pin.key!=UINT32_MAX && (pins.empty() || pins.back().key<pin.key));
            pin.package=packages[(size_t)owner];pin.bytes=number(bytes,at+8,8);pin.sha256=digest(bytes,at+16);
            pin.path=(root/(pin.sha256+".wav")).u8string();pins.push_back(std::move(pin));
        }
        auto result=AudioAssets::load(packages,pins,error,cancel);need(result!=nullptr);return result;
    }catch(...){error="Audio capsule could not be verified";return nullptr;}
}
}}
