#ifndef TSFP_MOD_FILES_HPP
#define TSFP_MOD_FILES_HPP
#include <filesystem>
#include <string>
namespace tsfp { namespace mods {
// Existing absolute, nonlinked local path. Directory endpoints on POSIX must
// belong to this user and deny other writers by default. Save namespaces apply
// a volume-aware permission check, including exFAT without native ACLs.
// Disabling directory ownership checks NEVER disables link/type/path checks.
std::filesystem::path checked_path(const std::string &,bool directory,bool private_directory=true);
bool sha256_text(const std::string &) noexcept;
}}
#endif
