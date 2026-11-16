#include "mod_audio_assets.hpp"
#include "mod_files.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace tsfp { namespace mods {
struct AudioAssets::Entry {
    uint32_t key;
    std::string snapshot;
    TsfpModWave wave{};
};
AudioAssets::AudioAssets()=default;
AudioAssets::~AudioAssets()=default;
namespace {
void need(bool value){if(!value)throw std::runtime_error("Invalid audio asset handoff");}
void running(const std::atomic<bool> *cancel){need(!cancel || !cancel->load());}
}
std::unique_ptr<const AudioAssets> AudioAssets::load(const std::vector<std::string> &packages,
    const std::vector<AudioAssetPin> &pins,std::string &error,const std::atomic<bool> *cancel) {
    error.clear();
    try {
        running(cancel);
        need(!packages.empty() && packages.size()<=32 && !pins.empty() && pins.size()<=asset_limit);
        std::set<std::string> authorized;std::set<uint32_t> keys;uint64_t total=0;
        for(const auto &package:packages)need(sha256_text(package) && authorized.insert(package).second);
        // Validate every declaration and the aggregate budget BEFORE reading data.
        for(const auto &pin:pins) {
            running(cancel);
            need(pin.key!=UINT32_MAX && keys.insert(pin.key).second && authorized.count(pin.package)==1 &&
                 sha256_text(pin.sha256) && pin.bytes>0 && pin.bytes<=TSFP_MOD_WAVE_MAX_BYTES &&
                 pin.bytes<=byte_limit-total);
            total+=pin.bytes;
        }
        auto result=std::unique_ptr<AudioAssets>(new AudioAssets);
        result->entries_.reserve(pins.size());
        for(const auto &pin:pins) {
            running(cancel);
            auto entry=std::unique_ptr<Entry>(new Entry);entry->key=pin.key;
            entry->snapshot=read_pinned_data_file(pin.path,pin.sha256,(size_t)pin.bytes,cancel);
            need(entry->snapshot.size()==pin.bytes);
            need(tsfp_mod_wave_parse(entry->snapshot.data(),entry->snapshot.size(),&entry->wave)==TSFP_MOD_WAVE_OK);
            result->entries_.push_back(std::move(entry));
        }
        running(cancel);
        std::sort(result->entries_.begin(),result->entries_.end(),[](const auto &a,const auto &b){return a->key<b->key;});
        result->bytes_=total;return result;
    }catch(...){error="Audio assets could not be verified";return nullptr;}
}
const TsfpModWave *AudioAssets::find(uint32_t key) const noexcept {
    auto entry=std::lower_bound(entries_.begin(),entries_.end(),key,
        [](const std::unique_ptr<Entry> &value,uint32_t wanted){return value->key<wanted;});
    return entry!=entries_.end() && (*entry)->key==key?&(*entry)->wave:nullptr;
}
}}
