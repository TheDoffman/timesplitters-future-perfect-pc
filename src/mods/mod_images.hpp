#ifndef TSFP_MOD_IMAGES_HPP
#define TSFP_MOD_IMAGES_HPP
#include <atomic>
#include <memory>
#include <string>
#include <vector>
namespace tsfp { namespace mods {
// Expected digests come from the trusted game build, NEVER a mod manifest.
struct ImagePins {
    std::string worker,guardian;
    bool valid() const noexcept;
};
class ImageStage {
public:
    static std::unique_ptr<ImageStage> create(const std::string &worker,const std::string &cache,
        const ImagePins &,const std::atomic<bool> *cancel,std::string &error);
    ~ImageStage();
    const std::string &worker() const noexcept{return worker_;}
    ImageStage(const ImageStage &)=delete;
    ImageStage &operator=(const ImageStage &)=delete;
private:
    ImageStage()=default;
    std::string directory_,worker_;
    std::vector<std::string> owned_;
};
}}
#endif
