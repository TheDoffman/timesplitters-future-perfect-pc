#ifndef TSFP_MOD_AUDIO_ASSETS_HPP
#define TSFP_MOD_AUDIO_ASSETS_HPP
#include "mod_wave.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace tsfp { namespace mods {
// Trusted launcher/bootstrap inputs AFTER catalog/package/active-profile checks.
// Neither an uploaded manifest nor the mod VM may supply its own authority.
// Resource keys are private mount-local IDs, not filenames or guest addresses.
struct AudioAssetPin {
    uint32_t key=UINT32_MAX;
    std::string package,path,sha256;
    uint64_t bytes=0;
};
class AudioAssets {
public:
    static constexpr size_t asset_limit=8192;
    static constexpr uint64_t byte_limit=256u*1024*1024;
    static std::unique_ptr<const AudioAssets> load(const std::vector<std::string> &authorized_packages,
        const std::vector<AudioAssetPin> &,std::string &error,const std::atomic<bool> *cancel=nullptr);
    // Borrowed immutable PCM, valid until this store is destroyed. Hold the store
    // until all voices are drained. Lookup does no IO, allocation, decoding/locks.
    const TsfpModWave *find(uint32_t key) const noexcept;
    size_t count() const noexcept{return entries_.size();}
    uint64_t bytes() const noexcept{return bytes_;}
    ~AudioAssets();
    AudioAssets(const AudioAssets &)=delete;
    AudioAssets &operator=(const AudioAssets &)=delete;
private:
    AudioAssets();
    struct Entry;
    std::vector<std::unique_ptr<Entry>> entries_;
    uint64_t bytes_=0;
};
}}
#endif
