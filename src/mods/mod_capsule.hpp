#ifndef TSFP_MOD_CAPSULE_HPP
#define TSFP_MOD_CAPSULE_HPP
#include <memory>
#include <string>
namespace tsfp { namespace mods {
// A source handoff from the trusted launcher, never a mod's own manifest.
// Launcher authenticates packages/catalog and supplies the expected capsule hash.
// This reader checks exact bytes/shape and derives save identity; it does not
// independently authenticate a publisher or authorize any helper/save path.
class SourceCapsule {
public:
    static constexpr size_t source_limit=1024u*1024;
    static constexpr size_t file_limit=source_limit+16+32*32;
    static std::unique_ptr<SourceCapsule> read(const std::string &path,const std::string &expected,std::string &error);
    const std::string &source()const noexcept{return source_;}
    const std::string &profile()const noexcept{return profile_;}
    const std::string &package()const noexcept{return package_;}
private:
    SourceCapsule()=default;
    std::string source_,profile_,package_;
};
}}
#endif
