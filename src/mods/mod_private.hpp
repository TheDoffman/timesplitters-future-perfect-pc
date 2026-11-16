#ifndef TSFP_MOD_PRIVATE_HPP
#define TSFP_MOD_PRIVATE_HPP
#include <memory>
#include <string>
namespace tsfp { namespace mods {
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
    PrivateDirectory()=default;
    std::string root_,parent_;
    void *owner_=nullptr;
};
}}
#endif
