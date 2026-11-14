#include "mod_images.hpp"
#include "mod_files.hpp"
#include <array>
#include <filesystem>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <objbase.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#else
#include <openssl/evp.h>
#endif
#endif
namespace tsfp { namespace mods {
namespace {
namespace fs=std::filesystem;
constexpr uint64_t image_limit=32u*1024*1024;
bool digest(const std::string &s) noexcept {
    if(s.size()!=64)return false;
    for(unsigned char c:s)if(!((c>='0' && c<='9') || (c>='a' && c<='f')))return false;
    return true;
}
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
fs::path plain_path(const std::string &text,bool directory,bool private_directory=true) {
    require(!text.empty() && text.size()<=32760 && text.find('\0')==std::string::npos,"Invalid helper path");
    auto path=fs::u8path(text);require(path.is_absolute(),"Helper paths must be absolute");
    for(const auto &part:path)require(part!="." && part!="..","Relative helper path component");
#ifdef _WIN32
    (void)private_directory;
    auto drive=path.root_name().wstring();
    require(drive.size()==2 && ((drive[0]>=L'A' && drive[0]<=L'Z') || (drive[0]>=L'a' && drive[0]<=L'z')) && drive[1]==L':',"Helper paths must be local");
    for(const auto &part:path)if(part!=path.root_name()) {
        auto name=part.wstring();
        require(name.find(L':')==std::wstring::npos && (name.empty() || (name.back()!=L'.' && name.back()!=L' ')),"Ambiguous Windows helper path");
    }
#endif
    bool first=true;
    for(auto cursor=path;;) {
#ifdef _WIN32
        DWORD attributes=GetFileAttributesW(cursor.c_str());
        require(attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&FILE_ATTRIBUTE_REPARSE_POINT),"Linked or missing helper path");
        require((!first || directory)?(attributes&FILE_ATTRIBUTE_DIRECTORY)!=0:(attributes&FILE_ATTRIBUTE_DIRECTORY)==0,"Invalid helper path type");
#else
        struct stat info{};require(lstat(cursor.c_str(),&info)==0 && !S_ISLNK(info.st_mode),"Linked or missing helper path");
        require((!first || directory)?S_ISDIR(info.st_mode):S_ISREG(info.st_mode),"Invalid helper path type");
        if(first && directory && private_directory)require(info.st_uid==geteuid() && !(info.st_mode&0022),"Helper cache is not private to its owner");
#endif
        auto parent=cursor.parent_path();if(parent==cursor || parent.empty())break;cursor=parent;first=false;
    }
    return path;
}
class Hash {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm_=nullptr;BCRYPT_HASH_HANDLE hash_=nullptr;std::vector<unsigned char> object_;
#elif defined(__APPLE__)
    CC_SHA256_CTX context_{};
#else
    EVP_MD_CTX *context_=nullptr;
#endif
public:
    Hash(){
#ifdef _WIN32
        require(BCryptOpenAlgorithmProvider(&algorithm_,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"Cannot open helper digest provider");
        DWORD size=0,returned=0;
        if(BCryptGetProperty(algorithm_,BCRYPT_OBJECT_LENGTH,(PUCHAR)&size,sizeof(size),&returned,0)<0 || returned!=sizeof(size) || !size || size>65536) {
            BCryptCloseAlgorithmProvider(algorithm_,0);algorithm_=nullptr;throw std::runtime_error("Cannot inspect helper digest provider");
        }
        try{object_.resize(size);}catch(...){BCryptCloseAlgorithmProvider(algorithm_,0);algorithm_=nullptr;throw;}
        if(BCryptCreateHash(algorithm_,&hash_,object_.data(),size,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(algorithm_,0);algorithm_=nullptr;throw std::runtime_error("Cannot initialize helper digest");}
#elif defined(__APPLE__)
        require(CC_SHA256_Init(&context_)==1,"Cannot initialize helper digest");
#else
        context_=EVP_MD_CTX_new();require(context_!=nullptr,"Cannot allocate helper digest");
        if(EVP_DigestInit_ex(context_,EVP_sha256(),nullptr)!=1){EVP_MD_CTX_free(context_);context_=nullptr;throw std::runtime_error("Cannot initialize helper digest");}
#endif
    }
    ~Hash(){
#ifdef _WIN32
        if(hash_)BCryptDestroyHash(hash_);if(algorithm_)BCryptCloseAlgorithmProvider(algorithm_,0);
#elif !defined(__APPLE__)
        if(context_)EVP_MD_CTX_free(context_);
#endif
    }
    void update(const unsigned char *p,size_t size){
#ifdef _WIN32
        require(BCryptHashData(hash_,const_cast<PUCHAR>(p),(ULONG)size,0)>=0,"Cannot hash helper image");
#elif defined(__APPLE__)
        require(CC_SHA256_Update(&context_,p,(CC_LONG)size)==1,"Cannot hash helper image");
#else
        require(EVP_DigestUpdate(context_,p,size)==1,"Cannot hash helper image");
#endif
    }
    std::string finish(){
        unsigned char bytes[32];
#ifdef _WIN32
        require(BCryptFinishHash(hash_,bytes,sizeof(bytes),0)>=0,"Cannot finish helper digest");
#elif defined(__APPLE__)
        require(CC_SHA256_Final(bytes,&context_)==1,"Cannot finish helper digest");
#else
        unsigned size=0;require(EVP_DigestFinal_ex(context_,bytes,&size)==1 && size==sizeof(bytes),"Cannot finish helper digest");
#endif
        const char *hex="0123456789abcdef";std::string out(64,'0');
        for(size_t i=0;i<sizeof(bytes);++i){out[i*2]=hex[bytes[i]>>4];out[i*2+1]=hex[bytes[i]&15];}return out;
    }
};
struct File {
#ifdef _WIN32
    HANDLE handle=INVALID_HANDLE_VALUE;
    ~File(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
#else
    int handle=-1;
    ~File(){if(handle>=0)close(handle);}
#endif
};
void copy_pinned(const fs::path &source,const fs::path &target,const std::string &expected,
                 std::vector<std::string> &owned,const std::atomic<bool> *cancel) {
    plain_path(source.u8string(),false);File input,output;uint64_t length=0;
#ifdef _WIN32
    input.handle=CreateFileW(source.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    require(input.handle!=INVALID_HANDLE_VALUE,"Cannot open helper image");BY_HANDLE_FILE_INFORMATION info{};
    require(GetFileType(input.handle)==FILE_TYPE_DISK && GetFileInformationByHandle(input.handle,&info) && !(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)),"Invalid opened helper image");
    length=((uint64_t)info.nFileSizeHigh<<32)|info.nFileSizeLow;
#else
    input.handle=open(source.c_str(),O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW);require(input.handle>=0,"Cannot open helper image");struct stat info{};
    require(fstat(input.handle,&info)==0 && S_ISREG(info.st_mode) && info.st_size>0,"Invalid opened helper image");length=(uint64_t)info.st_size;
#endif
    require(length && length<=image_limit,"Helper image exceeds size budget");
    // Reserve ownership tracking before creating anything that cleanup must own.
    owned.push_back(target.u8string());
#ifdef _WIN32
    output.handle=CreateFileW(target.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(output.handle==INVALID_HANDLE_VALUE){owned.pop_back();throw std::runtime_error("Cannot create private helper image");}
#else
    output.handle=open(target.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(output.handle<0){owned.pop_back();throw std::runtime_error("Cannot create private helper image");}
#endif
    Hash hash;std::array<unsigned char,32768> bytes{};uint64_t total=0;
    for(;;) {
        require(!cancel || !cancel->load(),"Helper verification cancelled");size_t count=0;
#ifdef _WIN32
        DWORD got=0;require(ReadFile(input.handle,bytes.data(),(DWORD)bytes.size(),&got,nullptr)!=FALSE,"Cannot read helper image");count=got;
#else
        ssize_t got=read(input.handle,bytes.data(),bytes.size());if(got<0 && errno==EINTR)continue;require(got>=0,"Cannot read helper image");count=(size_t)got;
#endif
        if(!count)break;
        require(total+count<=length,"Helper image changed while reading");hash.update(bytes.data(),count);
        size_t written=0;
        while(written<count) {
#ifdef _WIN32
            DWORD put=0;require(WriteFile(output.handle,bytes.data()+written,(DWORD)(count-written),&put,nullptr) && put,"Cannot stage helper image");written+=put;
#else
            ssize_t put=write(output.handle,bytes.data()+written,count-written);if(put<0 && errno==EINTR)continue;require(put>0,"Cannot stage helper image");written+=(size_t)put;
#endif
        }
        total+=count;
    }
    require(total==length && hash.finish()==expected,"Helper image digest mismatch");
#ifdef _WIN32
    require(FlushFileBuffers(output.handle)!=FALSE,"Cannot flush helper image");
    CloseHandle(output.handle);output.handle=INVALID_HANDLE_VALUE;
    require(SetFileAttributesW(target.c_str(),FILE_ATTRIBUTE_READONLY)!=FALSE,"Cannot protect helper image");
#else
    require(fsync(output.handle)==0 && fchmod(output.handle,0500)==0,"Cannot protect helper image");
#endif
}
}
std::filesystem::path checked_path(const std::string &text,bool directory,bool private_directory){return plain_path(text,directory,private_directory);}
bool sha256_text(const std::string &text) noexcept {return digest(text);}
bool ImagePins::valid()const noexcept {
#ifdef __APPLE__
    return digest(worker) && digest(guardian);
#else
    return digest(worker) && guardian.empty();
#endif
}
std::unique_ptr<ImageStage> ImageStage::create(const std::string &image,const std::string &cache,const ImagePins &pins,
    const std::atomic<bool> *cancel,std::string &error) {
    error.clear();
    try {
        require(pins.valid(),"Invalid trusted helper image pins");auto root=plain_path(cache,true);auto source=plain_path(image,false);
        auto stage=std::unique_ptr<ImageStage>(new ImageStage);
#ifdef _WIN32
        GUID guid;require(SUCCEEDED(CoCreateGuid(&guid)),"Cannot create helper staging identity");wchar_t text[40];
        require(StringFromGUID2(guid,text,40)>0,"Cannot format helper staging identity");auto directory=root/(std::wstring(L"images-")+text);
        stage->directory_=directory.u8string();
        if(!CreateDirectoryW(directory.c_str(),nullptr)){stage->directory_.clear();throw std::runtime_error("Cannot create private helper directory");}
        auto target=directory/L"tsfp-mod-worker.exe";
#else
        std::string pattern=(root/"images-XXXXXX").string();std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);
        // Allocate the stored path before mkdtemp so an allocation failure cannot orphan it.
        stage->directory_=pattern;
        if(!mkdtemp(name.data())){stage->directory_.clear();throw std::runtime_error("Cannot create private helper directory");}
        std::memcpy(stage->directory_.data(),name.data(),pattern.size());
        auto directory=fs::path(stage->directory_);auto target=directory/"tsfp-mod-worker";
#endif
        copy_pinned(source,target,pins.worker,stage->owned_,cancel);stage->worker_=target.u8string();
#ifdef __APPLE__
        copy_pinned(source.parent_path()/"tsfp-mod-guardian",directory/"tsfp-mod-guardian",pins.guardian,stage->owned_,cancel);
#endif
        return stage;
    }catch(const std::exception &){error="Trusted helper images could not be verified";return {};}
}
ImageStage::~ImageStage(){
    try {
    for(const auto &name:owned_) {
        std::error_code ec;auto path=fs::u8path(name);auto status=fs::symlink_status(path,ec);
        if(ec || !fs::is_regular_file(status) || fs::is_symlink(status))continue;
#ifdef _WIN32
        if(!SetFileAttributesW(path.c_str(),FILE_ATTRIBUTE_NORMAL))continue;
#endif
        fs::remove(path,ec);
    }
    if(!directory_.empty()){std::error_code ec;fs::remove(fs::u8path(directory_),ec);}
    }catch(...){} // Never unwind through a host shutdown/error path.
}
}}
#ifdef __APPLE__
#pragma clang diagnostic pop
#endif
