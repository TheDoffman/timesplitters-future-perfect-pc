#include "mod_images.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#endif
static int run(const std::string &mode,const std::string &image,const std::string &cache,const std::string &worker,const std::string &guardian) {
    tsfp::mods::ImagePins pins{worker,guardian=="-"?"":guardian};std::string error;
    std::atomic<bool> cancel{mode=="cancel"};auto stage=tsfp::mods::ImageStage::create(image,cache,pins,&cancel,error);
    bool accepted=stage!=nullptr,unchanged=false,readonly=false;
    if(stage) {
        auto path=std::filesystem::u8path(stage->worker());
        readonly=(std::filesystem::status(path).permissions()&std::filesystem::perms::owner_write)==std::filesystem::perms::none;
        if(mode=="snapshot") {
            std::ifstream input(path,std::ios::binary);std::string before{std::istreambuf_iterator<char>(input),{}};input.close();
            std::ofstream changed(std::filesystem::u8path(image),std::ios::binary|std::ios::trunc);changed<<"changed after verification";changed.close();
            std::ifstream second(path,std::ios::binary);std::string after{std::istreambuf_iterator<char>(second),{}};unchanged=before==after;
        }
        if(mode=="unknown") {std::ofstream keep(path.parent_path()/"keep.txt");keep<<"preserve unknown content";}
    }
    stage.reset();
    printf("{\"accepted\":%s,\"snapshot_unchanged\":%s,\"readonly\":%s}\n",accepted?"true":"false",unchanged?"true":"false",readonly?"true":"false");return 0;
}
#ifdef _WIN32
int wmain(int argc,wchar_t **argv) {
    if(argc!=6)return 2;std::string args[5];
    for(int i=0;i<5;++i){int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i+1],-1,nullptr,0,nullptr,nullptr);if(n<=0)return 2;
        args[i].resize((size_t)n);if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i+1],-1,args[i].data(),n,nullptr,nullptr))return 2;args[i].pop_back();}
    return run(args[0],args[1],args[2],args[3],args[4]);
}
#else
int main(int argc,char **argv){return argc==6?run(argv[1],argv[2],argv[3],argv[4],argv[5]):2;}
#endif
