#include "mod_capsule.hpp"
#include "mod_files.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace tsfp { namespace mods {
namespace {
void need(bool value){if(!value)throw std::runtime_error("Invalid script source handoff");}
uint32_t word(const std::string &s,size_t at) {
    uint32_t n=0;for(unsigned i=0;i<4;++i)n|=(uint32_t)(unsigned char)s[at+i]<<(8*i);return n;
}
std::string hex(const std::string &s,size_t at) {
    const char *alphabet="0123456789abcdef";std::string out(64,'0');
    for(size_t i=0;i<32;++i){auto c=(unsigned char)s[at+i];out[i*2]=alphabet[c>>4];out[i*2+1]=alphabet[c&15];}return out;
}
bool source_text(const std::string &s) {
    if(s.empty() || s.size()>SourceCapsule::source_limit || s.compare(0,4,"\x1bLua")==0)return false;
    size_t at=0;
    while(at<s.size()) {
        uint32_t value=(unsigned char)s[at++],minimum;unsigned count;
        if(value<128){if(!value)return false;continue;}
        if(value>=0xc2 && value<=0xdf){count=1;minimum=0x80;value&=31;}
        else if(value>=0xe0 && value<=0xef){count=2;minimum=0x800;value&=15;}
        else if(value>=0xf0 && value<=0xf4){count=3;minimum=0x10000;value&=7;}
        else return false;
        if(at+count>s.size())return false;
        for(unsigned i=0;i<count;++i){unsigned c=(unsigned char)s[at++];if((c&0xc0)!=0x80)return false;value=(value<<6)|(c&63);}
        if(value<minimum || value>0x10ffff || (value>=0xd800 && value<=0xdfff))return false;
    }
    return true;
}
}
std::unique_ptr<SourceCapsule> SourceCapsule::read(const std::string &path,const std::string &expected,std::string &error) {
    error.clear();
    try {
        auto bytes=read_pinned_file(path,expected,file_limit);
        need(bytes.size()>=16 && bytes.compare(0,8,"TSFPMOD1")==0);
        uint32_t count=word(bytes,8),size=word(bytes,12);
        need(count && count<=32 && size && size<=source_limit && bytes.size()==16u+count*32u+size);
        auto result=std::unique_ptr<SourceCapsule>(new SourceCapsule());
        result->package_=hex(bytes,16);std::string previous;
        for(uint32_t i=1;i<count;++i) {
            auto digest=hex(bytes,16+i*32);need(digest!=result->package_ && (previous.empty() || previous<digest));previous=digest;
        }
        result->source_=bytes.substr(16+count*32);need(source_text(result->source_));
        result->profile_=sha256_bytes(std::string("TSFP isolated mode saves v1\n")+bytes.substr(8,4)+bytes.substr(16,count*32));
        return result;
    }catch(...){error="Script source handoff could not be verified";return nullptr;}
}
}}
