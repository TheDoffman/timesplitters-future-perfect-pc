# Native Linux game integration

Native game bring-up on `feature/linux-x64`, continuing the verified shared-renderer
foundation. Reviewed Linux menus/first-level movement, firing and pause/resume
now pass bounded isolated runs; see STATUS for exact revisions and limits.

`tools/build/build-native-linux.py --game-dir PATH --setup --generate --test`
uses the same runtime/XBE/Python pins, complete patch stack and generator as
Windows/Mac. It keeps one game build in `build/linux/game`, with source/binary/
compatibility receipts in BUILD.json. Game data is read from the supplied
existing directory, not copied. Use Clang in the native Ubuntu working checkout.
Final patch2402 extends the title/input/audio guards after2401 and selects only
OS-specific Linux files. Shared SDL input/audio, binding/settings/debug models,
textures, gameplay patches and Xbox/Vulkan frontend compile directly.

`src/native/main.c` is the same native entry point on Mac/Linux. `mmio_linux.c`
maps Linux x64 ucontext registers to the bounded scalar MOV decoder in
`x64_access.h`; unknown instructions/address mismatches fail closed. ARM64 and
x64 adapters call one `mmio_device.h` policy for APU, CRTC acknowledgement and
AC97 reset, using existing writable aliases without page protection changes.
`tests/gameplay/native_mmio_linux.c` checks decoding and actual SIGSEGV recovery.

`crash_posix.c` retains the same shared mmap/helper/health/breadcrumb/report
pipeline. Darwin keeps Mach/CoreGraphics/CoreText/CommonCrypto services; Linux
uses ucontext, process_vm_readv, OpenSSL and addr2line. Linux guest-memory reads
may be denied by normal ptrace policy; per-region capture status records failure
without elevation. Linux hang sampling has no live stack provider and focus is
the last SDL observation, not an external desktop query. Linux ELF uses -no-pie
for deterministic addr2line addresses; report metadata says linux-x64.

`text_bitmap_linux.c` adapts Fontconfig/FreeType to the existing RHI bitmap
callbacks. `settings_linux.c` supplies folder dialogs and shared debug attachment;
the Linux editing/debug dialogs are pending (explicit self-test skip77). Common
in-game Video Options and controls JSON remain available, with unsupported
saved fields preserved. Its graphics check tests shared resolution apply/restore,
not GUI parity. `online_linux.c` supervises the same Python Online companion/
handshake and uses shared network policy; portable release packaging is pending.

`tools/testing/test-native-linux.py` bounds game runs to600s, locks
`private/test-saves/native-linux`, isolates all input/display/save/XDG/crash paths,
and skips startup movies like the routine Windows runner. `--capture` retains two
rotating BMPs under `private/captures/native-linux`; `--test-keys` consumes the
existing synthetic SDL event format. Test scripts never use personal configs/saves.
Successful presents and captures are separate from visually reviewed gameplay.
Initial Linux movie decoding is unavailable until the pinned FFmpeg integration
is adapted; this must stay explicit in manifests/documentation.

The shared native test-disk recycler also permits the two exact Linux test roots;
it still rejects redirected paths and unknown partition names. Completed tests
recycle only their known disposable partitions; user saves are never selected.

Use the project-owned guest_arena_service API for MMIO maps/aliases and test
arenas. The former w32_guest_* compatibility aliases were retired; native ARM64
and x64 fixtures now call the same canonical provider. Patch2403 imports OpenSSL
in the input subdirectory where its non-global imported target is consumed.

Linux shared guest pointer guards query current `/proc/self/maps` protection
boundaries. Known-size span guards use that provider; the resident-page PSAPI
optimization remains Windows-only. Snapshots retain their existing lifetime of
one uninterrupted host operation, never across guest calls/protection changes.

Patch2404 enables the existing Mac/Windows HLE graphics bridge on Linux,
including texture/state/vertex submission and shared frame capture. Native
MMIO fixtures link libm for the POSIX floating-point compatibility functions.
The auto-look span fixture also runs on Linux with real mmap/mprotect boundaries.
`test_native_crash.py` runs the shared fatal-signal/helper, watchdog suppression,
report contents and linked-root rejection checks on Linux as well as Darwin;
Linux fatal symbols use non-PIE ELF plus addr2line, with hang sampling explicit.
The Linux fixture faults at a named UD2 instruction to verify the captured
fault PC symbol; reports include both native and guest registers.
Patch2405 enables C++ linking at the Linux title scope and resolves the static
runtime library cycle with a GNU archive rescan group. Linux exposes the same
native Arcade-only Online mode contract as Mac; Story transport is deferred.
The Cocoa settings self-test is compiled only on Apple; Linux returns skip77.
Patch2406 routes native vblank production through writable aliases with atomic
OR, using `guest_mmio_interrupt.h`. The producer must set interrupt bits, not
execute the guest acknowledgement policy on a write-trapped CRTC page. Linux's
first game boot exposed that host RMW fault; the protected-MMIO fixture checks
production through the same helper before guest acknowledgement clears it.
Linux game runs use the same 500-entry translated-call trace budget as the Mac
runner, in addition to the hard aggregate log cap.
The x64 adapter also handles compiler-folded immediate TEST reads, preserving
registers and computing SF/ZF/PF/CF/OF. Protected APU fixture reads check both
branch outcomes; unsupported arithmetic instructions still fail closed.
`--unlocked-profile` prepares the same immutable Test Lab fixture used by
Windows/Mac in the locked runner's disposable `unlocked` child. Only its save,
input, feedback and texture environment is adopted; Linux display/XDG/crash
isolation remains active. The fixture contains five small host save files, not
virtual disk images, and is never edited by the game.

Direct WSLg delivery is now verified after an OS runtime update (WSL 3.0.3,
WSLg 1.0.79); the prior transparent COPY MODE windows came from virtiofs shared-
memory EIO, not renderer policy. Direct Wayland gameplay mouse capture/release
was confirmed by the user. See development/linux.md and STATUS for scope.
CPU llvmpipe remains a separate performance limit; direct delivery does not
prove RTX acceleration. Original 60 FPS is the recovery setting for testing.

The isolated Mesa 26.2.5 Dozen hardware probe detects the RTX 4070 SUPER but exposes
only Vulkan 1.2, without maintenance4. The shared production RHI contract rejects
it; no API/feature checks were weakened. System Mesa/project pins are unchanged.
The manual profile's 240 FPS beta choice was backed up and reset to Original 60 FPS.

Vulkan1.2 compatibility is in development in final patch2407. Devices must
expose dynamic-rendering/synchronization2 KHR extensions and timeline semaphores.
Descriptor sets come from per-recording pools reset only after GPU serial
completion; absent extended dynamic state is baked into pipeline keys. Fragment
interfaces receive full vertex output vectors and extract their original lanes
through the bounded SPIR-V adapter `src/graphics/vulkan_interface.h`. The existing
1.3 path remains, with `RECOMP_VK_COMPAT12` for software-driver regression checks.
No driver version spoofing or skipped required render policy is intended.

The isolated Dozen build carries `tools/build/mesa-dzn-granularity.diff`: graphics
and compute queues must report texel-granular copies (1,1,1), which its region
copy implementation supports. This changes neither feature bits nor API version.
The shared texture contract accepts the two permitted UNORM conversion integers,
then compares sampling against independent raw readback; half-float is bit-exact.
`tests/graphics/test_vulkan_interface.py` runs the bounded SPIR-V adapter fixture
without graphics hardware or game assets in the native shared CI suite.

Final patch2408 tracks each descriptor pool's 128-set capacity before allocating,
avoiding expected pool-exhaustion warnings during longer gameplay. The shared RHI
contract now checks varying widths 1/2/3 and 512 distinct uniform/scissor draws
without intervening flush, repeated across six submissions. All pixel values
must survive; validation stays enabled. `linux_vulkan_sdk.select_driver` supplies
WSL's installed D3D12 bridge search path for explicit ICD selection in both test
runners. `tools/build/build-wsl-vulkan.py` reproduces the private pinned driver
with publisher archive, patch, manifest and resulting-library hashes.
