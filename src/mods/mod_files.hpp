#ifndef TSFP_MOD_FILES_HPP
#define TSFP_MOD_FILES_HPP
#include <filesystem>
#include <string>
#include <atomic>
#include <chrono>
#include <cstdint>
namespace tsfp { namespace mods {
// Existing absolute, nonlinked local path. Directory endpoints on POSIX must
// belong to this user and deny other writers by default. Save namespaces apply
// a volume-aware permission check, including exFAT without native ACLs.
// Disabling directory ownership checks NEVER disables link/type/path checks.
std::filesystem::path checked_path(const std::string &,bool directory,bool private_directory=true);
bool sha256_text(const std::string &) noexcept;
std::string sha256_bytes(const std::string &);
// Recovery-only borrowed native descriptor. Caller owns/checks the file identity;
// this function neither opens paths nor closes handles. Reads are deadline/size bound.
bool verify_pinned_descriptor(uintptr_t,const std::string &,uint64_t maximum,
    std::chrono::steady_clock::time_point deadline)noexcept;
// Read and hash one opened regular file into an owned immutable-by-caller
// snapshot. Size is independently bounded (at most32MiB). No filesystem writes.
std::string read_pinned_file(const std::string &,const std::string &sha256,size_t maximum);
// Data-only snapshot, <=64MiB, from a single-linked regular file. Cancellation
// is checked between bounded reads. Expected hash comes from the trusted caller.
// This does not change the32MiB executable/source input policy above.
std::string read_pinned_data_file(const std::string &,const std::string &sha256,size_t maximum,
                                 const std::atomic<bool> *cancel=nullptr);
}}
#endif
