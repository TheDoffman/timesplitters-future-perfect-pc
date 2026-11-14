#ifndef TSFP_MOD_RESOURCES_HPP
#define TSFP_MOD_RESOURCES_HPP
#include "mod_images.hpp"
#include "mod_private.hpp"
#include <memory>
#include <string>
namespace tsfp { namespace mods {
// OS-derived paths only. The executable is the currently running native image,
// never argv[0], CWD, a package field or an environment helper-path override.
std::string native_cache_parent();
class RuntimeResources {
public:
    // subdir/name/pins are generated game constants. cache_parent is supplied
    // by the trusted OS adapter (tests use an isolated owned parent instead).
    static std::unique_ptr<RuntimeResources> create(const std::string &subdir,const std::string &name,
        const ImagePins &,const std::string &cache_parent,std::string &error);
    const std::string &worker()const noexcept{return images_->worker();}
    const std::string &cache()const noexcept{return cache_->root();}
    bool recheck(std::string &error)const{return cache_->recheck(error);}
private:
    std::unique_ptr<PrivateDirectory> cache_;
    std::unique_ptr<ImageStage> images_; // Destroy verified images before cache.
};
}}
#endif
