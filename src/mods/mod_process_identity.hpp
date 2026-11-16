#ifndef TSFP_MOD_PROCESS_IDENTITY_HPP
#define TSFP_MOD_PROCESS_IDENTITY_HPP
#include <cstdint>
#include <array>
#include <cstddef>
#include <string>

namespace tsfp { namespace mods {
// Trusted broker metadata only. These queries never terminate processes. The
// journal owner/layout still needs independent admission before any cleanup.
enum class ProcessStampKind:uint32_t { none=0,windows_filetime=1,linux_start_ticks=2,darwin_start_time=3 };
struct ProcessStamp {
    ProcessStampKind kind=ProcessStampKind::none;
    uint64_t pid=0,start_a=0,start_b=0;
    bool valid()const noexcept;
};
// Darwin records guardian then worker; other hosts record their one worker.
struct ProcessStamps {
    std::array<ProcessStamp,2> values{};
    size_t count=0;
    bool valid()const noexcept;
};
// Windows-only owned profile/cache artifacts. Absolute user paths are not stored.
struct ProcessArtifacts {
    std::string cache_leaf,profile,sid;
    uint64_t volume=0,file=0;
};
enum class ProcessState { alive,exited,unknown };
ProcessStampKind native_process_stamp_kind()noexcept;
// An exited/missing task may have no stamp. Permission/query/format errors are
// unknown, never evidence of death. A live sample includes its creation stamp.
ProcessState read_process_stamp(uint64_t pid,ProcessStamp &)noexcept;
// PID reuse with a different immutable creation stamp proves the recorded task
// exited. A same-stamp task or an uncertain query must be preserved.
ProcessState process_stamp_state(const ProcessStamp &)noexcept;
#ifdef _WIN32
// Snapshot an already-owned handle at creation, without reopening its PID.
// The handle is borrowed and is never closed or transferred by this function.
ProcessState read_owned_process_stamp(uintptr_t handle,ProcessStamp &)noexcept;
#endif
}}
#endif
