#include "mod_saves.hpp"
#include "mod_files.hpp"
#include <array>
#include <filesystem>
#include <stdexcept>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cerrno>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#ifdef __APPLE__
#include <sys/mount.h>
#else
#include <sys/vfs.h>
#endif
#endif
namespace tsfp { namespace mods {
namespace {
namespace fs=std::filesystem;
constexpr const char *base_marker="TSFP mod save namespaces v1\n";
constexpr const char *public_error="Mod saves could not be isolated; scripted play must not start";
void need(bool value){if(!value)throw std::runtime_error(public_error);}
#ifdef _WIN32
struct Local {void *p=nullptr;~Local(){if(p)LocalFree(p);}};
struct File {HANDLE value=INVALID_HANDLE_VALUE;~File(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
struct Security {
    std::vector<unsigned char> user;
    unsigned char system[SECURITY_MAX_SID_SIZE]{},admins[SECURITY_MAX_SID_SIZE]{};
    Local descriptor;SECURITY_ATTRIBUTES attributes{};
    Security() {
        File token;DWORD size=0;HANDLE raw=nullptr;
        need(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&raw)!=FALSE);token.value=raw;
        GetTokenInformation(token.value,TokenUser,nullptr,0,&size);need(size && size<=65536);user.resize(size);
        need(GetTokenInformation(token.value,TokenUser,user.data(),size,&size)!=FALSE);
        DWORD n=sizeof(system);need(CreateWellKnownSid(WinLocalSystemSid,nullptr,system,&n)!=FALSE);
        n=sizeof(admins);need(CreateWellKnownSid(WinBuiltinAdministratorsSid,nullptr,admins,&n)!=FALSE);
        LPWSTR text=nullptr;need(ConvertSidToStringSidW(sid(),&text)!=FALSE);Local converted;converted.p=text;
        std::wstring sddl=L"O:"+std::wstring(text)+L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;"+text+L")";
        need(ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&descriptor.p,nullptr)!=FALSE);
        attributes={sizeof(attributes),descriptor.p,FALSE};
    }
    PSID sid(){return reinterpret_cast<TOKEN_USER *>(user.data())->User.Sid;}
    bool principal(PSID value){return EqualSid(value,sid()) || EqualSid(value,system) || EqualSid(value,admins) || IsWellKnownSid(value,WinCreatorOwnerRightsSid);}
    void check(const fs::path &path,bool exact) {
        std::array<wchar_t,MAX_PATH> volume{},format{};DWORD flags=0;
        need(GetVolumePathNameW(path.c_str(),volume.data(),(DWORD)volume.size()) &&
            GetVolumeInformationW(volume.data(),nullptr,0,nullptr,nullptr,&flags,format.data(),(DWORD)format.size()));
        if(!(flags&FILE_PERSISTENT_ACLS)){need(_wcsicmp(format.data(),L"exFAT")==0);return;}
        Local descriptor_read;PSID owner=nullptr;PACL acl=nullptr;
        need(GetNamedSecurityInfoW(const_cast<LPWSTR>(path.c_str()),SE_FILE_OBJECT,
            OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,&owner,nullptr,&acl,nullptr,&descriptor_read.p)==ERROR_SUCCESS);
        need(owner && EqualSid(owner,sid()) && acl);
        if(exact) {
            SECURITY_DESCRIPTOR_CONTROL control{};DWORD revision=0;
            need(GetSecurityDescriptorControl(descriptor_read.p,&control,&revision) && (control&SE_DACL_PROTECTED));
            need(acl->AceCount==3);
        }
        unsigned seen=0;
        constexpr DWORD writes=GENERIC_ALL|GENERIC_WRITE|DELETE|WRITE_DAC|WRITE_OWNER|FILE_DELETE_CHILD|
            FILE_WRITE_DATA|FILE_APPEND_DATA|FILE_WRITE_EA|FILE_WRITE_ATTRIBUTES;
        for(DWORD i=0;i<acl->AceCount;++i) {
            void *raw=nullptr;need(GetAce(acl,i,&raw)!=FALSE);auto *header=static_cast<ACE_HEADER *>(raw);
            if(header->AceFlags&INHERIT_ONLY_ACE){need(!exact);continue;}
            if(header->AceType==ACCESS_DENIED_ACE_TYPE){need(!exact);continue;}
            need(header->AceType==ACCESS_ALLOWED_ACE_TYPE);
            auto *ace=static_cast<ACCESS_ALLOWED_ACE *>(raw);auto *who=&ace->SidStart;
            if(exact) {
                need(ace->Mask==FILE_ALL_ACCESS && (EqualSid(who,sid()) || EqualSid(who,system) || EqualSid(who,admins)));
                need(header->AceFlags==(OBJECT_INHERIT_ACE|CONTAINER_INHERIT_ACE));
                unsigned bit=EqualSid(who,sid())?1u:EqualSid(who,system)?2u:4u;need(!(seen&bit));seen|=bit;
            }else if(ace->Mask&writes)need(principal(who));
        }
        if(exact)need(seen==7u);
    }
};
#else
struct File {int value=-1;~File(){if(value>=0)close(value);}};
struct Security {
    void check(const fs::path &path,bool) {
        struct statfs volume{};need(statfs(path.c_str(),&volume)==0);
#ifdef __APPLE__
        if(strcmp(volume.f_fstypename,"exfat")==0)return;
#else
        if(volume.f_type==0x2011BAB0L)return; // Linux EXFAT_SUPER_MAGIC.
#endif
        struct stat s{};need(lstat(path.c_str(),&s)==0 && s.st_uid==geteuid() && !(s.st_mode&0022));
    }
};
#endif
void regular(const fs::path &path,Security &security) {
    checked_path(path.u8string(),false);security.check(path,false);
#ifdef _WIN32
    File input;input.value=CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,
        OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);need(input.value!=INVALID_HANDLE_VALUE);
    BY_HANDLE_FILE_INFORMATION info{};need(GetFileType(input.value)==FILE_TYPE_DISK && GetFileInformationByHandle(input.value,&info) && info.nNumberOfLinks==1 &&
        !(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)));
#else
    struct stat s{};need(lstat(path.c_str(),&s)==0 && S_ISREG(s.st_mode) && s.st_nlink==1);
#endif
}
void marker(const fs::path &path,const std::string &expected,bool create,Security &security) {
    if(create) {
        File output;
#ifdef _WIN32
        output.value=CreateFileW(path.c_str(),GENERIC_WRITE,0,&security.attributes,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        need(output.value!=INVALID_HANDLE_VALUE);DWORD size=0;
        need(WriteFile(output.value,expected.data(),(DWORD)expected.size(),&size,nullptr) && size==expected.size() && FlushFileBuffers(output.value));
#else
        output.value=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);need(output.value>=0);
        size_t at=0;while(at<expected.size()) {
            ssize_t n=write(output.value,expected.data()+at,expected.size()-at);
            if(n<0 && errno==EINTR)continue;
            need(n>0);at+=(size_t)n;
        }
        need(fsync(output.value)==0);
#endif
    }
    regular(path,security);need(fs::file_size(path)==expected.size());
    File input;std::array<char,128> bytes{};
#ifdef _WIN32
    input.value=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    need(input.value!=INVALID_HANDLE_VALUE);DWORD got=0;
    need(ReadFile(input.value,bytes.data(),(DWORD)bytes.size(),&got,nullptr) && got==expected.size());
#else
    input.value=open(path.c_str(),O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW);need(input.value>=0);
    ssize_t got;do{got=read(input.value,bytes.data(),bytes.size());}while(got<0 && errno==EINTR);
    need(got==(ssize_t)expected.size());
#endif
    need(memcmp(bytes.data(),expected.data(),expected.size())==0);
}
void directory(const fs::path &path,const std::string &identity,Security &security) {
    // An interrupted/unrecognized directory is preserved, never adopted or erased.
#ifdef _WIN32
    bool created=CreateDirectoryW(path.c_str(),&security.attributes)!=FALSE;
    if(!created)need(GetLastError()==ERROR_ALREADY_EXISTS);
#else
    bool created=mkdir(path.c_str(),0700)==0;
    if(!created)need(errno==EEXIST);
#endif
    checked_path(path.u8string(),true,false);security.check(path,true);
    marker(path/".tsfp-mod-saves",identity,created,security);
}
void tree(const fs::path &root,Security &security,bool leased) {
    size_t entries=0;
    for(fs::recursive_directory_iterator it(root),end;it!=end;++it) {
        need(++entries<=4096 && it.depth()<16);
        auto path=it->path();need(path.u8string().size()<4096);
        auto kind=it->symlink_status();
        if(fs::is_directory(kind)){checked_path(path.u8string(),true,false);security.check(path,false);}
        else {
            need(fs::is_regular_file(kind));
            if(leased && path==root/".session-lock")checked_path(path.u8string(),false);
            else regular(path,security);
        }
    }
}
struct Lease {
    File file;
    explicit Lease(const fs::path &path,Security &security) {
#ifdef _WIN32
        file.value=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,&security.attributes,OPEN_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        need(file.value!=INVALID_HANDLE_VALUE);BY_HANDLE_FILE_INFORMATION info{};
        need(GetFileType(file.value)==FILE_TYPE_DISK && GetFileInformationByHandle(file.value,&info) && info.nNumberOfLinks==1 && !info.nFileSizeHigh && !info.nFileSizeLow &&
            !(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)));
#else
        file.value=open(path.c_str(),O_RDWR|O_CREAT|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW,0600);need(file.value>=0);
        struct stat s{};need(fstat(file.value,&s)==0 && S_ISREG(s.st_mode) && s.st_nlink==1 && !s.st_size);
        need(flock(file.value,LOCK_EX|LOCK_NB)==0);
#endif
        security.check(path,false);
    }
    void check(const fs::path &path,Security &security)const {
        security.check(path,false);
#ifdef _WIN32
        BY_HANDLE_FILE_INFORMATION info{};
        need(GetFileInformationByHandle(file.value,&info) && info.nNumberOfLinks==1 && !info.nFileSizeHigh && !info.nFileSizeLow);
#else
        struct stat held{},named{};
        need(fstat(file.value,&held)==0 && lstat(path.c_str(),&named)==0 && held.st_dev==named.st_dev && held.st_ino==named.st_ino &&
            held.st_nlink==1 && !held.st_size);
#endif
    }
};
void verify(const fs::path &root,const fs::path &parent,const std::string &identity,Security &security,bool leased=false) {
    checked_path(parent.u8string(),true,false);security.check(parent,false);
    checked_path((parent/"ModSaves").u8string(),true,false);security.check(parent/"ModSaves",true);
    marker(parent/"ModSaves/.tsfp-mod-saves",base_marker,false,security);
    checked_path(root.u8string(),true,false);security.check(root,true);
    marker(root/".tsfp-mod-saves",identity,false,security);tree(root,security,leased);
}
}
std::unique_ptr<SaveNamespace> SaveNamespace::open(const std::string &originals,const std::string &profile,std::string &error) {
    error.clear();
    try {
        need(sha256_text(profile) && originals.size()<3900);
        auto original=fs::u8path(originals);need(original.has_filename() && original.filename()!="ModSaves");
        for(const auto &part:original) {
            auto folded=part.u8string();for(auto &c:folded)if(c>='A' && c<='Z')c=static_cast<char>(c+'a'-'A');
            need(folded!="modsaves");
        }
        // Validate the absent original leaf too: no dot/alias components allowed.
        auto parent=checked_path(original.parent_path().u8string(),true,false);Security security;security.check(parent,false);
        need(original.filename()!="." && original.filename()!=".." && original.filename().u8string().find('\0')==std::string::npos);
#ifdef _WIN32
        auto name=original.filename().wstring();need(name.find(L':')==std::wstring::npos && name.back()!=L'.' && name.back()!=L' ');
#endif
        if(fs::exists(original) || fs::is_symlink(fs::symlink_status(original)))checked_path(original.u8string(),true,false);
        auto root=parent/"ModSaves"/profile;
        need(root.u8string().size()<4096);
        auto result=std::unique_ptr<SaveNamespace>(new SaveNamespace());
        result->root_=root.u8string();result->parent_=parent.u8string();result->marker_="TSFP mod saves v1\n"+profile+"\n";
        directory(parent/"ModSaves",base_marker,security);directory(root,result->marker_,security);
        // Inspect before opening the lease (Windows intentionally denies sharing).
        verify(root,parent,result->marker_,security);
        result->lock_=new Lease(root/".session-lock",security);
        need(result->recheck(error));
        return result;
    }catch(...){error=public_error;return nullptr;}
}
bool SaveNamespace::recheck(std::string &error)const {
    error.clear();
    try {
        need(lock_!=nullptr);Security security;
        // The held Windows lock cannot be reopened; validate it through its
        // owned handle and inspect every other persistent member normally.
        static_cast<Lease *>(lock_)->check(fs::u8path(root_)/".session-lock",security);
        verify(fs::u8path(root_),fs::u8path(parent_),marker_,security,true);return true;
    }catch(...){error=public_error;return false;}
}
SaveNamespace::~SaveNamespace(){delete static_cast<Lease *>(lock_);}
}}
