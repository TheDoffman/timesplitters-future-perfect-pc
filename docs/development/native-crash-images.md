# Native crash image locations

POSIX native reports retain bounded executable-image metadata alongside the
register context. A program counter is resolved against the image containing
that address; it is never blindly attributed to the game executable. This is
diagnostic context, not proof of a crash's cause or a complete stack unwind.

`src/diagnostics/pc_native_modules.h` provides the shared range registry:
1024 entries, 512 bytes per path, immutable payloads published through lock-free
atomics, and an atomic live bit for unload. Interrupted/unpublished records are
ignored. Full or contended publication marks the view incomplete without waiting.
Truncated paths are displayed but are not passed to symbolication tools.

The Darwin adapter records executable Mach-O segments through normal dyld
add/remove notifications, outside the signal handler. Linux's separate reporter
reads the faulting process's `/proc/<pid>/maps` and verifies ELF64 load segments
before calculating the link address, covering PIE/shared-library load bias.
Unavailable maps, anonymous executable memory, deleted/escaped filenames and
unknown ranges remain unrecorded. They do not trigger a guessed main-image lookup.
Windows continues using its existing minidump module data.

The faulting thread still only copies registers, notifies its prestarted reporter
and waits within the existing bound. It does not walk the loader, open files,
allocate or symbolize. The reporter includes a bounded image listing in
`native-stack.txt`, uses the matching PC image for `atos`/`addr2line`, and records
the ARM64 LR image separately. Matching files/dSYMs remain necessary; image
information does not guarantee that a file has not changed since loading.
Existing private report permissions, local-only export, size/tool time limits,
guest-memory permission checks and report schema remain unchanged.

`tests/diagnostics/test_crash_capture.py::test_native_module_registry` runs the
portable boundary cases. `tests/gameplay/test_native_crash.py` additionally
loads a Unicode/space-named test library after reporter startup and faults inside
its exported function, checking the actual image and symbol in the report.
Native Mac/Linux checks and real game reproduction are required separately.

The public lifecycle's earlier Mac menu signal11 remains undetermined. This
change improves future evidence; it does not claim to fix that crash.
