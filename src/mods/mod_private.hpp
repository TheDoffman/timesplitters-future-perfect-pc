#ifndef TSFP_MOD_PRIVATE_HPP
#define TSFP_MOD_PRIVATE_HPP
#include <memory>
#include <string>
namespace tsfp { namespace mods {
struct ImagePins;
class Process;
class CacheJournal;
bool cache_process_start_allowed(const std::string &)noexcept;
// Fresh private directory under a trusted existing per-user parent. Unlike save
// namespaces, executable caches require native permissions; no exFAT fallback.
// No existing directory is adopted. New caches hold an exclusive, non-inherited
// lifetime lock and a marker bound to their generated leaf name. Destruction
// removes these records only after all payloads are gone, then removes the owned
// directory by held identity. Unknown contents/changed records are preserved.
class PrivateDirectory {
public:
    static std::unique_ptr<PrivateDirectory> create(const std::string &parent,std::string &error);
    ~PrivateDirectory();
    const std::string &root()const noexcept{return root_;}
    bool recheck(std::string &error)const;
    PrivateDirectory(const PrivateDirectory &)=delete;
    PrivateDirectory &operator=(const PrivateDirectory &)=delete;
private:
    friend class CacheJournal;
    friend bool cache_process_start_allowed(const std::string &)noexcept;
    PrivateDirectory()=default;
    std::string root_,parent_;
    void *owner_=nullptr;
};
// Bounded, new-only per-start evidence, tied to a live PrivateDirectory lease.
// A committed record precedes transmission of mod source to the confined child.
class CacheJournal {
public:
    static std::unique_ptr<CacheJournal> begin(const std::string &cache,const std::string &image,const ImagePins *,std::string &error);
    ~CacheJournal();
    bool starting(std::string &error)noexcept;
    bool commit(const Process &,std::string &error);
    void finish(bool artifacts_cleaned)noexcept;
    CacheJournal(const CacheJournal &)=delete;
    CacheJournal &operator=(const CacheJournal &)=delete;
private:
    struct State;
    std::unique_ptr<State> state_;
    CacheJournal();
    friend bool cache_process_start_allowed(const std::string &)noexcept;
};
}}
#endif
