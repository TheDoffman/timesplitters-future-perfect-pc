// Native diagnostic/developer supervisor. The game will use the Process interface
// from its asynchronous bridge; this batch CLI never applies gameplay commands.
#include "mod_process.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <exception>
#include <thread>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#endif
namespace {
int run(const std::string &image,const std::string &cache) {
    constexpr size_t input_limit=4u*1024*1024,output_limit=1024u*1024,diagnostic_limit=4096;
    std::vector<unsigned char> input;std::array<unsigned char,16384> buffer{};
    for(;;) {
        size_t count=fread(buffer.data(),1,buffer.size(),stdin);
        if(!count)break;
        if(input.size()+count>input_limit){fputs("Supervisor input budget exceeded\n",stderr);return 125;}
        input.insert(input.end(),buffer.data(),buffer.data()+count);
    }
    std::string error;auto child=tsfp::mods::Process::start(image,cache,error);
    if(!child){fprintf(stderr,"%s\n",error.c_str());return 125;}
    auto start=std::chrono::steady_clock::now();size_t offset=0,total=0,diagnostic=0;bool input_closed=false;
    // Buffer before forwarding, so hostile native output never grows memory or
    // blocks this supervisor on a stopped downstream reader while the worker lives.
    std::vector<unsigned char> output;tsfp::mods::ProcessUsage usage;
    for(;;) {
        if(std::chrono::steady_clock::now()-start>std::chrono::seconds(2)){error="Worker exceeded independent wall deadline";break;}
        if(!child->usage(usage)){error="Cannot inspect worker resources";break;}
        if(usage.resident_bytes>128u*1024*1024 || usage.cpu_microseconds>1500000){error="Worker exceeded independent resource budget";break;}
        if(offset<input.size()) {
            int count=child->write(input.data()+offset,input.size()-offset);
            if(count<0 && !usage.exited){error="Worker input pipe failed";break;}
            if(count>0)offset+=(size_t)count;
        }
        if(!input_closed && offset==input.size()){child->close_input();input_closed=true;}
        int count=child->read(buffer.data(),buffer.size());
        if(count<0){error="Worker output pipe failed";break;}
        if(count>0) {
            total+=(size_t)count;if(total>output_limit){error="Worker output budget exceeded";break;}
            output.insert(output.end(),buffer.data(),buffer.data()+count);
        }
        int noise=child->read(buffer.data(),buffer.size(),true);
        if(noise<0){error="Worker diagnostic pipe failed";break;}
        if(noise>0){diagnostic+=(size_t)noise;if(diagnostic>diagnostic_limit){error="Worker diagnostic budget exceeded";break;}}
        if(usage.exited && count==0 && noise==0) {
            if(diagnostic){error="Worker wrote unexpected diagnostics";break;}
            child.reset();
            if(!output.empty() && fwrite(output.data(),1,output.size(),stdout)!=output.size())return 125;
            return usage.exit_code;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    child.reset();fprintf(stderr,"%s\n",error.c_str());return 125;
}
int guarded_run(const std::string &image,const std::string &cache) {
    try{return run(image,cache);}
    catch(const std::exception &){fputs("Supervisor allocation or platform failure\n",stderr);return 125;}
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t **argv) {
    if(argc!=3)return 2;
    if(_setmode(_fileno(stdin),_O_BINARY)<0 || _setmode(_fileno(stdout),_O_BINARY)<0)return 2;
    auto utf8=[](const wchar_t *text){int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,nullptr,0,nullptr,nullptr);
        if(size<=0)return std::string();std::string result((size_t)size,0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,&result[0],size,nullptr,nullptr);result.pop_back();return result;};
    return guarded_run(utf8(argv[1]),utf8(argv[2]));
}
#else
int main(int argc,char **argv){return argc==3?guarded_run(argv[1],argv[2]):2;}
#endif
