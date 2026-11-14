#ifndef TSFP_MOD_SAVES_HPP
#define TSFP_MOD_SAVES_HPP
#include <memory>
#include <string>
namespace tsfp { namespace mods {
// Startup owns this lease until ALL guest storage and match workers stop.
// No original saves are copied or written. Files persist across lease lifetimes.
class SaveNamespace {
public:
    static std::unique_ptr<SaveNamespace> open(const std::string &original_saves,
        const std::string &profile_sha256,std::string &error);
    ~SaveNamespace();
    const std::string &root()const noexcept{return root_;}
    // Before storage initialization / subsequent match start. A failure must
    // abort scripted play, NEVER silently fall back to the original save root.
    bool recheck(std::string &error)const;
    SaveNamespace(const SaveNamespace &)=delete;
    SaveNamespace &operator=(const SaveNamespace &)=delete;
private:
    SaveNamespace()=default;
    std::string root_,parent_,marker_;
    void *lock_=nullptr;
};
}}
#endif
