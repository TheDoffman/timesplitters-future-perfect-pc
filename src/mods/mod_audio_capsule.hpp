#ifndef TSFP_MOD_AUDIO_CAPSULE_HPP
#define TSFP_MOD_AUDIO_CAPSULE_HPP
#include "mod_audio_assets.hpp"
namespace tsfp { namespace mods {
// Private launcher handoff, not an uploaded package format. The caller supplies
// independently authorized active package identities and the capsule hash.
// Paths are derived exclusively from a trusted cache root and payload digests.
std::unique_ptr<const AudioAssets> read_audio_capsule(const std::string &path,
    const std::string &sha256,const std::string &cache_root,
    const std::vector<std::string> &authorized_packages,std::string &error,
    const std::atomic<bool> *cancel=nullptr);
}}
#endif
