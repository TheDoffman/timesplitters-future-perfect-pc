#include "mod_resources.hpp"
#include "mod_files.hpp"
#include <array>
#include <filesystem>
#include <stdexcept>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif
namespace tsfp { namespace mods {
namespace {
namespace fs=std::filesystem;
void need(bool condition){if(!condition)throw std::runtime_error("Native script runtime unavailable");}
fs::path executable() {
#ifdef _WIN32
    std::array<wchar_t,32768> path{};DWORD n=GetModuleFileNameW(nullptr,path.data(),(DWORD)path.size());
    need(n && n<path.size());return checked_path(fs::path(path.data()).u8string(),false);
#elif defined(__APPLE__)
    std::array<char,32768> path{};uint32_t n=(uint32_t)path.size();need(_NSGetExecutablePath(path.data(),&n)==0);
    // Canonicalize only the path reported by the OS for this running image.
    // Uploaded paths never pass through this trusted OS alias-resolution step.
    return checked_path(fs::canonical(fs::path(path.data())).u8string(),false);
#else
    std::array<char,32768> path{};ssize_t n=readlink("/proc/self/exe",path.data(),path.size());
    need(n>0 && (size_t)n<path.size());return checked_path(std::string(path.data(),(size_t)n),false);
#endif
}
}
std::string native_cache_parent() {
#ifdef _WIN32
    std::array<wchar_t,MAX_PATH> path{};
    need(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,path.data())));
    return checked_path(fs::path(path.data()).u8string(),true).u8string();
#else
    struct passwd entry{},*found=nullptr;std::array<char,16384> buffer{};
    need(getpwuid_r(geteuid(),&entry,buffer.data(),buffer.size(),&found)==0 && found && entry.pw_dir && *entry.pw_dir);
    auto home=checked_path(entry.pw_dir,true);
#ifdef __APPLE__
    auto cache=home/"Library/Caches";
#else
    auto cache=home/".cache";
#endif
    std::error_code error;bool exists=fs::exists(cache,error);need(!error);
    return checked_path((exists?cache:home).u8string(),true).u8string();
#endif
}
std::unique_ptr<RuntimeResources> RuntimeResources::create(const std::string &subdir,const std::string &name,
    const ImagePins &pins,const std::string &parent,std::string &error) {
    error.clear();
    try {
        need(subdir.size()==75 && subdir.compare(0,11,"ModRuntime/")==0 && sha256_text(subdir.substr(11)) && pins.valid());
#ifdef _WIN32
        need(name=="tsfp-mod-worker.exe");
#else
        need(name=="tsfp-mod-worker");
#endif
        auto image=checked_path((executable().parent_path()/subdir/name).u8string(),false);
        auto result=std::unique_ptr<RuntimeResources>(new RuntimeResources);
        result->cache_=PrivateDirectory::create(parent,error);if(!result->cache_)return nullptr;
        result->images_=ImageStage::create(image.u8string(),result->cache_->root(),pins,nullptr,error);
        if(!result->images_ || !result->cache_->recheck(error))return nullptr;
        return result;
    }catch(...){error="Compiled native script helpers could not be prepared";return nullptr;}
}
}}
