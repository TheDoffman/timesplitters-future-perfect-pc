#ifndef TSFP_MOD_PROCESS_IDENTITY_HPP
#define TSFP_MOD_PROCESS_IDENTITY_HPP
#include <cstdint>

namespace tsfp { namespace mods {
// Trusted broker metadata only. These queries never terminate processes. The
// journal owner/layout still needs independent admission before any cleanup.
enum class ProcessStampKind:uint32_t { none=0,windows_filetime=1,linux_start_ticks=2,darwin_start_time=3 };
struct ProcessStamp {
    ProcessStampKind kind=ProcessStampKind::none;
    uint64_t pid=0,start_a=0,start_b=0;
    bool valid()const noexcept;
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
