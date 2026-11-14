#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "mod_process.hpp"
#include <windows.h>
#include <userenv.h>
#include <sddl.h>
#include <aclapi.h>
#include <psapi.h>
#include <objbase.h>
#include <filesystem>
#include <vector>
#include <array>
#include <algorithm>

namespace tsfp { namespace mods {
namespace {
std::wstring wide(const std::string &text) {
    if(text.empty() || text.size()>32760 || text.find('\0')!=std::string::npos)return {};
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),(int)text.size(),nullptr,0);
    if(count<=0)return {};std::wstring result((size_t)count,0);
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),(int)text.size(),&result[0],count)!=count)return {};
    return result;
}
std::string last_error(const char *operation){return std::string(operation)+" (Windows error "+std::to_string(GetLastError())+")";}
void close(HANDLE &handle){if(handle && handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);handle=nullptr;}
bool plain_local_path(const std::wstring &text,bool directory) {
    std::filesystem::path path(text);
    for(const auto &part:path)if(part==L"." || part==L"..")return false;
    bool first=true;
    for(;;) {
        DWORD attributes=GetFileAttributesW(path.c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
        if((!first || directory) && !(attributes&FILE_ATTRIBUTE_DIRECTORY))return false;
        auto parent=path.parent_path();if(parent==path || parent.empty())return true;
        path=parent;first=false;
    }
}
struct Pipe {
    HANDLE parent=nullptr,child=nullptr,event=nullptr;OVERLAPPED pending{};bool active=false,writing=false;
    std::array<unsigned char,65536> buffer{};DWORD buffered=0,offset=0;
    ~Pipe(){disconnect();close(child);close(event);}
    bool create(const std::wstring &name,bool parentWrites) {
        writing=parentWrites;
        parent=CreateNamedPipeW(name.c_str(),(parentWrites?PIPE_ACCESS_OUTBOUND:PIPE_ACCESS_INBOUND)|FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,65536,65536,0,nullptr);
        if(parent==INVALID_HANDLE_VALUE){parent=nullptr;return false;}
        event=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!event)return false;
        OVERLAPPED connection{};connection.hEvent=event;
        BOOL connected=ConnectNamedPipe(parent,&connection);DWORD error=connected?ERROR_SUCCESS:GetLastError();
        if(!connected && error!=ERROR_IO_PENDING && error!=ERROR_PIPE_CONNECTED)return false;
        SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
        child=CreateFileW(name.c_str(),parentWrites?GENERIC_READ:GENERIC_WRITE,0,&security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(child==INVALID_HANDLE_VALUE){
            child=nullptr;
            if(error==ERROR_IO_PENDING){CancelIoEx(parent,&connection);DWORD ignored;GetOverlappedResult(parent,&connection,&ignored,TRUE);}
            return false;
        }
        if(error==ERROR_IO_PENDING){DWORD count;if(!GetOverlappedResult(parent,&connection,&count,TRUE))return false;}
        ResetEvent(event);return true;
    }
    int transfer(void *bytes,size_t length) {
        if(!parent || !length)return 0;
        if(writing) {
            if(active) {
                DWORD count=0;if(!GetOverlappedResult(parent,&pending,&count,FALSE)) {
                    if(GetLastError()==ERROR_IO_INCOMPLETE)return 0;return -1;
                }
                active=false;return (int)count;
            }
            DWORD size=(DWORD)std::min(length,buffer.size());memcpy(buffer.data(),bytes,size);
            pending={};pending.hEvent=event;ResetEvent(event);DWORD count=0;
            if(WriteFile(parent,buffer.data(),size,&count,&pending))return (int)count;
            if(GetLastError()!=ERROR_IO_PENDING)return -1;active=true;return 0;
        }
        if(buffered>offset) {
            DWORD count=(DWORD)std::min(length,(size_t)(buffered-offset));memcpy(bytes,buffer.data()+offset,count);offset+=count;return (int)count;
        }
        if(active) {
            DWORD count=0;if(!GetOverlappedResult(parent,&pending,&count,FALSE)) {
                DWORD error=GetLastError();if(error==ERROR_IO_INCOMPLETE)return 0;
                if(error==ERROR_BROKEN_PIPE)return 0;return -1;
            }
            active=false;buffered=count;offset=0;
            if(count)return transfer(bytes,length);return 0;
        }
        pending={};pending.hEvent=event;ResetEvent(event);DWORD count=0;
        if(ReadFile(parent,buffer.data(),(DWORD)buffer.size(),&count,&pending)){buffered=count;offset=0;return count?transfer(bytes,length):0;}
        DWORD error=GetLastError();if(error==ERROR_BROKEN_PIPE)return 0;
        if(error!=ERROR_IO_PENDING)return -1;active=true;return 0;
    }
    void disconnect(){if(parent && active){CancelIoEx(parent,&pending);DWORD ignored;GetOverlappedResult(parent,&pending,&ignored,TRUE);active=false;}close(parent);}
};
// Only newly-created profile/cache objects get these ACLs. Deny writes and ACL
// changes for the package SID, retain broker/user rights, and never grant world access.
bool read_only_package(const std::wstring &path,PSID sid) {
    PSECURITY_DESCRIPTOR descriptor=nullptr;PACL old=nullptr,next=nullptr;
    if(GetNamedSecurityInfoW(path.c_str(),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,&old,nullptr,&descriptor)!=ERROR_SUCCESS)return false;
    EXPLICIT_ACCESSW entries[2]{};
    entries[0].grfAccessPermissions=FILE_WRITE_DATA|FILE_APPEND_DATA|FILE_WRITE_EA|FILE_WRITE_ATTRIBUTES|DELETE|WRITE_DAC|WRITE_OWNER;
    entries[0].grfAccessMode=DENY_ACCESS;entries[0].grfInheritance=SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    entries[1].grfAccessPermissions=FILE_GENERIC_READ|FILE_GENERIC_EXECUTE;
    entries[1].grfAccessMode=GRANT_ACCESS;entries[1].grfInheritance=SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    for(auto &entry:entries){entry.Trustee.TrusteeForm=TRUSTEE_IS_SID;entry.Trustee.TrusteeType=TRUSTEE_IS_UNKNOWN;entry.Trustee.ptstrName=(LPWSTR)sid;}
    bool okay=SetEntriesInAclW(2,entries,old,&next)==ERROR_SUCCESS && SetNamedSecurityInfoW((LPWSTR)path.c_str(),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,next,nullptr)==ERROR_SUCCESS;
    if(next)LocalFree(next);if(descriptor)LocalFree(descriptor);return okay;
}
class WindowsProcess final:public Process {
public:
    Pipe input,output,diagnostic;HANDLE process=nullptr,job=nullptr;PSID sid=nullptr;
    std::wstring profile,directory,image,profile_root;bool profile_owned=false,directory_owned=false,image_owned=false;
    ~WindowsProcess() override {
        terminate();input.disconnect();output.disconnect();diagnostic.disconnect();
        if(image_owned)DeleteFileW(image.c_str());if(directory_owned)RemoveDirectoryW(directory.c_str());
        if(profile_owned)DeleteAppContainerProfile(profile.c_str());if(sid)FreeSid(sid);
    }
    int write(const void *bytes,size_t length)override{return input.transfer(const_cast<void *>(bytes),length);}
    int read(void *bytes,size_t length,bool error)override{return (error?diagnostic:output).transfer(bytes,length);}
    void close_input()override{input.disconnect();}
    bool usage(ProcessUsage &result)override {
        if(!process)return false;DWORD code;if(!GetExitCodeProcess(process,&code))return false;
        result.exited=code!=STILL_ACTIVE;result.exit_code=(int)code;
        PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);
        if(!result.exited && !GetProcessMemoryInfo(process,(PROCESS_MEMORY_COUNTERS *)&memory,sizeof(memory))) {
            if(!GetExitCodeProcess(process,&code) || code==STILL_ACTIVE)return false;
            result.exited=true;result.exit_code=(int)code;
        }
        result.resident_bytes=memory.WorkingSetSize;
        FILETIME created,exited,kernel,user;
        if(!GetProcessTimes(process,&created,&exited,&kernel,&user))return false;
        ULARGE_INTEGER a,b;a.LowPart=kernel.dwLowDateTime;a.HighPart=kernel.dwHighDateTime;b.LowPart=user.dwLowDateTime;b.HighPart=user.dwHighDateTime;
        result.cpu_microseconds=(a.QuadPart+b.QuadPart)/10;return true;
    }
    void terminate()override {
        if(job)TerminateJobObject(job,125);
        if(process){if(WaitForSingleObject(process,2000)!=WAIT_OBJECT_0){TerminateProcess(process,125);WaitForSingleObject(process,2000);}close(process);}
        close(job);
    }
};
}
std::unique_ptr<Process> Process::start(const std::string &executable,const std::string &cache,std::string &error) {
    auto child=std::make_unique<WindowsProcess>();auto source=wide(executable),root=wide(cache);
    if(source.empty() || root.empty() || !std::filesystem::path(source).is_absolute() || !std::filesystem::path(root).is_absolute() ||
       source.rfind(L"\\\\",0)==0 || root.rfind(L"\\\\",0)==0){error="Worker and cache need absolute local paths";return {};}
    std::error_code fs;
    if(!plain_local_path(source,false) || !plain_local_path(root,true) || !std::filesystem::is_regular_file(source,fs) || std::filesystem::file_size(source,fs)>32u*1024*1024 || fs){error="Invalid trusted worker image or cache path";return {};}
    GUID guid;if(FAILED(CoCreateGuid(&guid))){error="Cannot create worker identity";return {};}
    wchar_t id[40];if(!StringFromGUID2(guid,id,40)){error="Cannot format worker identity";return {};}
    std::wstring unique=id;unique.erase(std::remove(unique.begin(),unique.end(),L'{'),unique.end());unique.erase(std::remove(unique.begin(),unique.end(),L'}'),unique.end());
    child->profile=L"TSFP.Mod."+unique;
    HRESULT created=CreateAppContainerProfile(child->profile.c_str(),L"TimeSplitters script worker",L"Temporary confined mod worker",nullptr,0,&child->sid);
    if(FAILED(created)){error="Cannot create worker AppContainer (HRESULT "+std::to_string((unsigned)created)+")";return {};}
    child->profile_owned=true;
    child->directory=(std::filesystem::path(root)/unique).wstring();
    if(!CreateDirectoryW(child->directory.c_str(),nullptr)){error=last_error("Cannot create private worker cache");return {};}
    child->directory_owned=true;
    if(!read_only_package(child->directory,child->sid)){error=last_error("Cannot restrict worker cache");return {};}
    child->image=(std::filesystem::path(child->directory)/L"worker.exe").wstring();
    if(!CopyFileW(source.c_str(),child->image.c_str(),TRUE)){error=last_error("Cannot stage worker image");return {};}
    child->image_owned=true;
    if(!SetFileAttributesW(child->image.c_str(),FILE_ATTRIBUTE_NORMAL) || !read_only_package(child->image,child->sid)){error=last_error("Cannot restrict read-only worker image");return {};}
    // Block the otherwise-writable per-profile filesystem. Only the broker owns it.
    LPWSTR sid_text=nullptr,profile_folder=nullptr;
    if(!ConvertSidToStringSidW(child->sid,&sid_text)){error="Cannot inspect worker SID";return {};}
    HRESULT folder_result=GetAppContainerFolderPath(sid_text,&profile_folder);LocalFree(sid_text);
    if(FAILED(folder_result) || !profile_folder){error="Cannot locate isolated worker profile";return {};}
    child->profile_root=profile_folder;
    std::filesystem::path profile_scope(profile_folder);CoTaskMemFree(profile_folder);
    // The API returns the AC subfolder on current Windows. Cover its siblings
    // only when the parent name proves this is the newly-owned profile root.
    if(_wcsicmp(profile_scope.filename().c_str(),L"AC")==0 && _wcsicmp(profile_scope.parent_path().filename().c_str(),child->profile.c_str())==0)profile_scope=profile_scope.parent_path();
    bool secured=read_only_package(profile_scope.wstring(),child->sid);
    for(std::filesystem::recursive_directory_iterator it(profile_scope,fs),end;secured && it!=end && !fs;it.increment(fs)) {
        if(it->is_symlink(fs) || fs || !read_only_package(it->path().wstring(),child->sid))secured=false;
    }
    if(fs)secured=false;
    if(!secured){error="Cannot restrict isolated worker profile";return {};}
    std::wstring pipe=L"\\\\.\\pipe\\TSFP.Mod."+unique;
    if(!child->input.create(pipe+L".in",true) || !child->output.create(pipe+L".out",false) || !child->diagnostic.create(pipe+L".err",false)){error=last_error("Cannot create worker pipes");return {};}
    child->job=CreateJobObjectW(nullptr,nullptr);if(!child->job){error=last_error("Cannot create worker job");return {};}
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_ACTIVE_PROCESS|JOB_OBJECT_LIMIT_PROCESS_MEMORY|JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
    limits.BasicLimitInformation.ActiveProcessLimit=1;limits.ProcessMemoryLimit=128u*1024*1024;
    if(!SetInformationJobObject(child->job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))){error=last_error("Cannot limit worker job");return {};}
    SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,4,0,&size);std::vector<unsigned char> storage(size);
    auto attributes=(LPPROC_THREAD_ATTRIBUTE_LIST)storage.data();if(!InitializeProcThreadAttributeList(attributes,4,0,&size)){error=last_error("Cannot initialize worker attributes");return {};}
    SECURITY_CAPABILITIES capabilities{};capabilities.AppContainerSid=child->sid;
    HANDLE handles[]={child->input.child,child->output.child,child->diagnostic.child};DWORD lpac=1;
    DWORD64 mitigation=PROCESS_CREATION_MITIGATION_POLICY_PROHIBIT_DYNAMIC_CODE_ALWAYS_ON|PROCESS_CREATION_MITIGATION_POLICY_WIN32K_SYSTEM_CALL_DISABLE_ALWAYS_ON;
    bool configured=UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,&capabilities,sizeof(capabilities),nullptr,nullptr) &&
        UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof(handles),nullptr,nullptr) &&
        UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_ALL_APPLICATION_PACKAGES_POLICY,&lpac,sizeof(lpac),nullptr,nullptr) &&
        UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,&mitigation,sizeof(mitigation),nullptr,nullptr);
    if(!configured){error=last_error("Cannot configure worker isolation attributes");DeleteProcThreadAttributeList(attributes);return {};}
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput=handles[0];startup.StartupInfo.hStdOutput=handles[1];startup.StartupInfo.hStdError=handles[2];startup.lpAttributeList=attributes;
    wchar_t system[MAX_PATH];UINT system_length=GetWindowsDirectoryW(system,MAX_PATH);
    if(!system_length || system_length>=MAX_PATH){error="Cannot locate system runtime";DeleteProcThreadAttributeList(attributes);return {};}
    std::wstring environment;
    auto variable=[&](const std::wstring &name,const std::wstring &value){environment+=name+L"="+value;environment.push_back(0);};
    variable(L"APPDATA",child->profile_root);variable(L"LANG",L"C");variable(L"LC_ALL",L"C");variable(L"LOCALAPPDATA",child->profile_root);
    variable(L"SystemDrive",std::wstring(system,system_length>=2?2:0));variable(L"SystemRoot",system);
    variable(L"TEMP",child->profile_root);variable(L"TMP",child->profile_root);variable(L"USERPROFILE",child->profile_root);variable(L"WINDIR",system);environment.push_back(0);
    std::wstring command=L"\""+child->image+L"\"";PROCESS_INFORMATION info{};
    bool launched=configured && system_length && system_length<MAX_PATH && CreateProcessW(child->image.c_str(),&command[0],nullptr,nullptr,TRUE,
        EXTENDED_STARTUPINFO_PRESENT|CREATE_UNICODE_ENVIRONMENT|CREATE_SUSPENDED|CREATE_NO_WINDOW,environment.data(),child->directory.c_str(),&startup.StartupInfo,&info);
    DWORD launch_error=GetLastError();DeleteProcThreadAttributeList(attributes);
    if(!launched){SetLastError(launch_error);error=last_error("Cannot launch confined worker");return {};}
    child->process=info.hProcess;
    bool assigned=AssignProcessToJobObject(child->job,child->process)!=FALSE;
    bool resumed=assigned && ResumeThread(info.hThread)!=DWORD(-1);CloseHandle(info.hThread);
    if(!resumed){error=last_error("Cannot attach worker job before execution");return {};}
    close(child->input.child);close(child->output.child);close(child->diagnostic.child);
    return child;
}
}}
#endif
