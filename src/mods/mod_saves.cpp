#include "mod_saves.hpp"
#include "mod_files.hpp"
#include "mod_private.hpp"
#include "mod_images.hpp"
#include "mod_process.hpp"
#include <atomic>
#include <map>
#include <mutex>
#include <array>
#include <filesystem>
#include <stdexcept>
#include <vector>
#include <cstdint>
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
#include <aclapi.h>
#include <sddl.h>
#include <objbase.h>
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
    bool allow_exfat;
    std::vector<unsigned char> user;
    unsigned char system[SECURITY_MAX_SID_SIZE]{},admins[SECURITY_MAX_SID_SIZE]{};
    Local descriptor;SECURITY_ATTRIBUTES attributes{};
    explicit Security(bool allow=true):allow_exfat(allow) {
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
        if(!(flags&FILE_PERSISTENT_ACLS)){need(allow_exfat && _wcsicmp(format.data(),L"exFAT")==0);return;}
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
    bool allow_exfat;
    explicit Security(bool allow=true):allow_exfat(allow){}
    void check(const fs::path &path,bool) {
        struct statfs volume{};need(statfs(path.c_str(),&volume)==0);
#ifdef __APPLE__
        if(strcmp(volume.f_fstypename,"exfat")==0){need(allow_exfat);return;}
#else
        if(volume.f_type==0x2011BAB0L){need(allow_exfat);return;} // Linux EXFAT_SUPER_MAGIC.
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
    explicit Lease(const fs::path &path,Security &security,bool fresh=false) {
#ifdef _WIN32
        file.value=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE|(fresh?DELETE:0),0,&security.attributes,
            fresh?CREATE_NEW:OPEN_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        need(file.value!=INVALID_HANDLE_VALUE);BY_HANDLE_FILE_INFORMATION info{};
        need(GetFileType(file.value)==FILE_TYPE_DISK && GetFileInformationByHandle(file.value,&info) && info.nNumberOfLinks==1 && !info.nFileSizeHigh && !info.nFileSizeLow &&
            !(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)));
#else
        file.value=open(path.c_str(),O_RDWR|O_CREAT|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW|(fresh?O_EXCL:0),0600);need(file.value>=0);
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
    void remove_fresh(const fs::path &path,Security &security) {
        check(path,security);
#ifdef _WIN32
        FILE_DISPOSITION_INFO disposition{};disposition.DeleteFile=TRUE;
        need(SetFileInformationByHandle(file.value,FileDispositionInfo,&disposition,sizeof(disposition))!=FALSE);
        CloseHandle(file.value);file.value=INVALID_HANDLE_VALUE;
#else
        need(unlink(path.c_str())==0);
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

namespace {
struct PrivateOwner {
    File directory;
    std::unique_ptr<Lease> lease;
    std::string marker;
#ifndef _WIN32
    File parent;
#endif
};
struct PrivateRegistry {
    std::mutex mutex;
    std::map<std::string,PrivateDirectory *> owners;
};
// Lifetime is process-wide; cache destruction may run from a detached session.
PrivateRegistry &private_registry(){static auto *value=new PrivateRegistry;return *value;}
thread_local CacheJournal *starting_journal=nullptr;
std::atomic<uint64_t> journal_sequence{0};
}
std::unique_ptr<PrivateDirectory> PrivateDirectory::create(const std::string &parent_name,std::string &error) {
    error.clear();
    try {
        auto parent=checked_path(parent_name,true);Security security(false);security.check(parent,false);
        auto result=std::unique_ptr<PrivateDirectory>(new PrivateDirectory);
        result->parent_=parent.u8string();result->owner_=new PrivateOwner;
        auto &owner=*static_cast<PrivateOwner *>(result->owner_);
#ifdef _WIN32
        GUID id;need(SUCCEEDED(CoCreateGuid(&id)));wchar_t text[40];need(StringFromGUID2(id,text,40)>0);
        auto path=parent/(std::wstring(L".tsfp-runtime-")+text);result->root_=path.u8string();
        if(!CreateDirectoryW(path.c_str(),&security.attributes)){result->root_.clear();throw std::runtime_error("Fresh cache creation failed");}
        // Keep delete sharing closed so a different object cannot replace the
        // owned directory while worker children use names underneath it.
        owner.directory.value=CreateFileW(path.c_str(),DELETE|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,
            OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        need(owner.directory.value!=INVALID_HANDLE_VALUE);
#else
        owner.parent.value=open(parent.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);need(owner.parent.value>=0);
        result->root_=(parent/".tsfp-runtime-XXXXXX").string();
        std::vector<char> text(result->root_.begin(),result->root_.end());text.push_back(0);
        if(!mkdtemp(text.data())){result->root_.clear();throw std::runtime_error("Fresh cache creation failed");}
        memcpy(result->root_.data(),text.data(),result->root_.size());
        owner.directory.value=open(result->root_.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);need(owner.directory.value>=0);
#endif
        // New caches carry an exclusive, non-inherited lifetime lease and an
        // explicit ownership marker. Unknown/legacy directories remain unmarked.
        owner.marker="TSFP mod runtime cache v2\n"+fs::u8path(result->root_).filename().u8string()+"\n";
        marker(fs::u8path(result->root_)/".tsfp-runtime-owner",owner.marker,true,security);
        owner.lease=std::make_unique<Lease>(fs::u8path(result->root_)/".runtime-lock",security,true);
        need(result->recheck(error));
        auto &registry=private_registry();std::lock_guard<std::mutex> guard(registry.mutex);
        need(registry.owners.size()<64 && registry.owners.emplace(result->root_,result.get()).second);
        return result;
    }catch(...){error="A private native helper cache could not be created";return nullptr;}
}
bool PrivateDirectory::recheck(std::string &error)const {
    error.clear();
    try {
        need(owner_ && !root_.empty());auto path=checked_path(root_,true);
        checked_path(parent_,true);Security security(false);security.check(fs::u8path(parent_),false);security.check(path,true);
        auto &owner=*static_cast<PrivateOwner *>(owner_);
        need(owner.lease!=nullptr);
        marker(path/".tsfp-runtime-owner",owner.marker,false,security);
        owner.lease->check(path/".runtime-lock",security);
#ifdef _WIN32
        BY_HANDLE_FILE_INFORMATION held{};
        need(owner.directory.value!=INVALID_HANDLE_VALUE && GetFileType(owner.directory.value)==FILE_TYPE_DISK &&
            GetFileInformationByHandle(owner.directory.value,&held) && (held.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && !(held.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT));
#else
        struct stat held{},named{};
        need(owner.directory.value>=0 && owner.parent.value>=0 && fstat(owner.directory.value,&held)==0 &&
            fstatat(owner.parent.value,path.filename().c_str(),&named,AT_SYMLINK_NOFOLLOW)==0 &&
            S_ISDIR(held.st_mode) && S_ISDIR(named.st_mode) && held.st_dev==named.st_dev && held.st_ino==named.st_ino);
#endif
        return true;
    }catch(...){error="Private native helper cache changed";return false;}
}
PrivateDirectory::~PrivateDirectory() {
    {auto &registry=private_registry();std::lock_guard<std::mutex> guard(registry.mutex);registry.owners.erase(root_);}
    auto *owner=static_cast<PrivateOwner *>(owner_);if(!owner)return;
    try {
        if(!root_.empty()) {
            if(owner->lease) {
                // A changed marker or any unknown payload preserves the whole
                // namespace. Never remove ownership evidence from uncertain data.
                std::string error;need(recheck(error));auto path=fs::u8path(root_);
                size_t count=0;
                for(const auto &entry:fs::directory_iterator(path)) {
                    auto name=entry.path().filename().u8string();
                    need(++count<=2 && (name==".tsfp-runtime-owner" || name==".runtime-lock"));
                }
                need(count==2);Security security(false);
                owner->lease->remove_fresh(path/".runtime-lock",security);
#ifdef _WIN32
                need(DeleteFileW((path/L".tsfp-runtime-owner").c_str())!=FALSE);
#else
                need(unlinkat(owner->directory.value,".tsfp-runtime-owner",0)==0);
#endif
            }
#ifdef _WIN32
            if(owner->directory.value!=INVALID_HANDLE_VALUE) {
                // The kernel refuses nonempty directories; never enumerate or
                // recursively erase an unexpected file left in this cache.
                FILE_DISPOSITION_INFO disposition{};disposition.DeleteFile=TRUE;
                (void)SetFileInformationByHandle(owner->directory.value,FileDispositionInfo,&disposition,sizeof(disposition));
            }
#else
            struct stat held{},named{};auto leaf=fs::u8path(root_).filename();
            if(owner->directory.value>=0 && owner->parent.value>=0 && fstat(owner->directory.value,&held)==0 &&
                fstatat(owner->parent.value,leaf.c_str(),&named,AT_SYMLINK_NOFOLLOW)==0 &&
                S_ISDIR(named.st_mode) && held.st_dev==named.st_dev && held.st_ino==named.st_ino)
                (void)unlinkat(owner->parent.value,leaf.c_str(),AT_REMOVEDIR);
#endif
        }
    }catch(...){} // Preserve uncertain/unknown state; no cleanup retry.
    delete owner;
}

namespace {
// Held handles prevent Windows replacement. POSIX names are resolved relative to
// the already-owned root descriptor, then matched to the original inode/content.
struct JournalFile {
    File file;std::string name,bytes;uint64_t device=0,inode=0;
    void create(PrivateOwner &owner,const fs::path &root,const std::string &leaf,const std::string &body) {
        need(body.size()<=4096);name=leaf;bytes=body;
#ifdef _WIN32
        (void)owner;Security security(false);
        file.value=CreateFileW((root/fs::u8path(name)).c_str(),GENERIC_READ|GENERIC_WRITE|DELETE,FILE_SHARE_READ,
            &security.attributes,CREATE_NEW,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        need(file.value!=INVALID_HANDLE_VALUE);DWORD written=0;
        need(WriteFile(file.value,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr) && written==bytes.size() && FlushFileBuffers(file.value));
        BY_HANDLE_FILE_INFORMATION info{};need(GetFileInformationByHandle(file.value,&info)!=FALSE);
        device=info.dwVolumeSerialNumber;inode=(uint64_t(info.nFileIndexHigh)<<32)|info.nFileIndexLow;
#else
        (void)root;file.value=openat(owner.directory.value,name.c_str(),O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        need(file.value>=0);struct stat info{};need(fstat(file.value,&info)==0);device=info.st_dev;inode=info.st_ino;
        size_t at=0;while(at<bytes.size()) {
            ssize_t n=write(file.value,bytes.data()+at,bytes.size()-at);
            if(n<0 && errno==EINTR)continue;
            need(n>0);at+=static_cast<size_t>(n);
        }
        need(fsync(file.value)==0 && fsync(owner.directory.value)==0);
#endif
        check(owner,root);
    }
    void check(PrivateOwner &owner,const fs::path &root)const {
        std::array<char,4097> buffer{};
#ifdef _WIN32
        (void)owner;Security security(false);security.check(root/fs::u8path(name),true);
        BY_HANDLE_FILE_INFORMATION info{};need(GetFileInformationByHandle(file.value,&info)!=FALSE && GetFileType(file.value)==FILE_TYPE_DISK &&
            info.nNumberOfLinks==1 && !(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) &&
            !info.nFileSizeHigh && info.nFileSizeLow==bytes.size() && info.dwVolumeSerialNumber==device &&
            ((uint64_t(info.nFileIndexHigh)<<32)|info.nFileIndexLow)==inode);
        LARGE_INTEGER zero{};need(SetFilePointerEx(file.value,zero,nullptr,FILE_BEGIN)!=FALSE);DWORD got=0;
        need(ReadFile(file.value,buffer.data(),static_cast<DWORD>(buffer.size()),&got,nullptr) && got==bytes.size());
#else
        (void)root;struct stat held{},named{};
        need(fstat(file.value,&held)==0 && fstatat(owner.directory.value,name.c_str(),&named,AT_SYMLINK_NOFOLLOW)==0 &&
            S_ISREG(held.st_mode) && S_ISREG(named.st_mode) && held.st_nlink==1 && held.st_uid==geteuid() && (held.st_mode&0777)==0600 &&
            uint64_t(held.st_dev)==device && uint64_t(held.st_ino)==inode && held.st_dev==named.st_dev && held.st_ino==named.st_ino &&
            held.st_size==static_cast<off_t>(bytes.size()));
        size_t at=0;while(at<bytes.size()) {
            ssize_t n=pread(file.value,buffer.data()+at,bytes.size()-at,static_cast<off_t>(at));
            if(n<0 && errno==EINTR)continue;
            need(n>0);at+=static_cast<size_t>(n);
        }
#endif
        need(memcmp(buffer.data(),bytes.data(),bytes.size())==0);
    }
    void remove(PrivateOwner &owner,const fs::path &root) {
        check(owner,root);
#ifdef _WIN32
        FILE_DISPOSITION_INFO disposition{};disposition.DeleteFile=TRUE;
        need(SetFileInformationByHandle(file.value,FileDispositionInfo,&disposition,sizeof(disposition))!=FALSE);
        CloseHandle(file.value);file.value=INVALID_HANDLE_VALUE;
#else
        need(unlinkat(owner.directory.value,name.c_str(),0)==0 && fsync(owner.directory.value)==0);
#endif
    }
};
bool plain_cache(const std::string &cache) {
    std::error_code error;auto status=fs::symlink_status(fs::u8path(cache)/".tsfp-runtime-owner",error);
    return (!error || error==std::errc::no_such_file_or_directory) && status.type()==fs::file_type::not_found;
}
bool ascii_line(const std::string &text,size_t maximum) {
    if(text.empty() || text.size()>maximum)return false;
    for(unsigned char c:text)if(c<33 || c>126)return false;
    return true;
}
}
struct CacheJournal::State {
    std::string cache;JournalFile intent,committed;ProcessStamps stamps;
    bool owned=false,attempted=false,claimed=false,complete=false,finished=false;
};
CacheJournal::CacheJournal():state_(new State){}
CacheJournal::~CacheJournal(){if(starting_journal==this)starting_journal=nullptr;}
std::unique_ptr<CacheJournal> CacheJournal::begin(const std::string &cache,const std::string &image,const ImagePins *pins,std::string &error) {
    error.clear();
    try {
        auto journal=std::unique_ptr<CacheJournal>(new CacheJournal);auto &state=*journal->state_;state.cache=cache;
        auto &registry=private_registry();std::lock_guard<std::mutex> guard(registry.mutex);
        auto found=registry.owners.find(cache);
        if(found==registry.owners.end()){need(plain_cache(cache));return journal;}
        auto *directory=found->second;need(directory->recheck(error) && pins && pins->valid());
        auto root=fs::u8path(cache),worker=fs::u8path(image);
        auto relative=worker.lexically_relative(root);auto components=std::distance(relative.begin(),relative.end());
        need(components==2 && relative.begin()->u8string().rfind("images-",0)==0 && relative.filename()==worker.filename());
        auto relative_text=relative.generic_u8string();need(ascii_line(relative_text,256) && relative_text.find("..") == std::string::npos);
        size_t entries=0;for(const auto &entry:fs::directory_iterator(root)){(void)entry;need(++entries<=256);}
        uint64_t sequence=journal_sequence.fetch_add(1)+1;need(sequence!=0);
        std::string leaf=".spawn-"+std::to_string(sequence);
        auto &owner=*static_cast<PrivateOwner *>(directory->owner_);uint64_t device=0,inode=0;
#ifdef _WIN32
        BY_HANDLE_FILE_INFORMATION identity{};need(GetFileInformationByHandle(owner.directory.value,&identity)!=FALSE);
        device=identity.dwVolumeSerialNumber;inode=(uint64_t(identity.nFileIndexHigh)<<32)|identity.nFileIndexLow;
#else
        struct stat identity{};need(fstat(owner.directory.value,&identity)==0);device=identity.st_dev;inode=identity.st_ino;
#endif
        need(inode!=0);
        std::string body="TSFP mod spawn intent v1\n"+root.filename().u8string()+"\n"+leaf+"\n"+
            std::to_string(device)+" "+std::to_string(inode)+"\n"+relative_text+"\n"+pins->worker+"\n"+pins->guardian+"\n";
        state.owned=true;state.intent.create(owner,root,leaf+".intent",body);
        return journal;
    }catch(...){error="Cannot record private worker startup intent";return {};}
}
bool CacheJournal::starting(std::string &error)noexcept {
    try {
        auto &state=*state_;need(!state.attempted && !state.finished && !starting_journal);
        state.attempted=true;starting_journal=this;return true;
    }catch(...){error="Cannot admit private worker startup";return false;}
}
bool cache_process_start_allowed(const std::string &cache)noexcept {
    try {
        auto &registry=private_registry();std::lock_guard<std::mutex> guard(registry.mutex);
        auto found=registry.owners.find(cache);
        if(found==registry.owners.end())return plain_cache(cache);
        if(!starting_journal)return false;
        auto &state=*starting_journal->state_;std::string error;
        need(state.owned && state.cache==cache && state.attempted && !state.claimed && !state.finished && found->second->recheck(error));
        state.intent.check(*static_cast<PrivateOwner *>(found->second->owner_),fs::u8path(cache));state.claimed=true;return true;
    }catch(...){return false;}
}
bool CacheJournal::commit(const Process &process,std::string &error) {
    try {
        auto &state=*state_;need(state.attempted && !state.complete && !state.finished);
        if(!state.owned){state.complete=true;if(starting_journal==this)starting_journal=nullptr;return true;}
        need(state.claimed && process.creation_stamps(state.stamps) && state.stamps.valid());
        std::string body="TSFP mod spawn commit v1\n"+fs::u8path(state.cache).filename().u8string()+"\n"+state.intent.name+"\n";
        body+=std::to_string(state.stamps.count)+"\n";
        for(size_t i=0;i<state.stamps.count;++i) {
            const auto &stamp=state.stamps.values[i];body+=std::to_string(static_cast<uint32_t>(stamp.kind))+" "+std::to_string(stamp.pid)+" "+std::to_string(stamp.start_a)+" "+std::to_string(stamp.start_b)+"\n";
        }
#ifdef _WIN32
        auto *artifact=process.creation_artifacts();need(artifact && ascii_line(artifact->cache_leaf,36) && artifact->cache_leaf.size()==36 &&
            artifact->profile=="TSFP.Mod."+artifact->cache_leaf && ascii_line(artifact->sid,192) && artifact->sid.rfind("S-1-15-2-",0)==0 && artifact->file);
        body+=artifact->cache_leaf+"\n"+artifact->profile+"\n"+artifact->sid+"\n"+std::to_string(artifact->volume)+" "+std::to_string(artifact->file)+"\n";
#endif
        auto &registry=private_registry();std::lock_guard<std::mutex> guard(registry.mutex);
        auto found=registry.owners.find(state.cache);need(found!=registry.owners.end() && found->second->recheck(error));
        auto &owner=*static_cast<PrivateOwner *>(found->second->owner_);auto root=fs::u8path(state.cache);
        state.intent.check(owner,root);
        auto name=state.intent.name.substr(0,state.intent.name.size()-7)+".commit";
        state.committed.create(owner,root,name,body);state.complete=true;
        if(starting_journal==this)starting_journal=nullptr;
        return true;
    }catch(...){if(starting_journal==this)starting_journal=nullptr;error="Cannot commit private worker creation evidence";return false;}
}
void CacheJournal::finish(bool artifacts_cleaned)noexcept {
    if(starting_journal==this)starting_journal=nullptr;
    try {
        auto &state=*state_;need(!state.finished);state.finished=true;
        if(!state.owned)return;
        need(state.complete && artifacts_cleaned);
        for(size_t i=0;i<state.stamps.count;++i)need(process_stamp_state(state.stamps.values[i])==ProcessState::exited);
        auto &registry=private_registry();std::lock_guard<std::mutex> guard(registry.mutex);std::string error;
        auto found=registry.owners.find(state.cache);need(found!=registry.owners.end() && found->second->recheck(error));
        auto &owner=*static_cast<PrivateOwner *>(found->second->owner_);auto root=fs::u8path(state.cache);
        state.intent.check(owner,root);state.committed.check(owner,root);
        state.committed.remove(owner,root);state.intent.remove(owner,root);
    }catch(...){} // Ambiguous startup, changed evidence or live/unknown child: preserve.
}
}}
