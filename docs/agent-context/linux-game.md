# Native Linux game integration

## Portable declaration guard

Main's patch2512 is exactly the d3d8_internal.h guard already contained in2401.
The loader omits2512 when2401 is present, while preserving2512 for stacks without
Linux. Its immutable patch remains tracked; effective-order/index coverage
records this one explicit duplicate rather than applying the same hunk twice.

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
callbacks. `settings_linux.c` supplies GTK settings/debug editing and the SDL
folder picker, using shared validation, bindings, display and debug models.
Unsupported saved fields are preserved. Capture-to-bind is still pending;
bindings can be edited through dropdowns. Native UI self-tests are distinct
from shared resolution apply/restore and physical desktop acceptance. `online_linux.c` supervises the same Python Online companion/
handshake and uses shared network policy; local standalone preview packaging
exists, while public release acceptance remains pending.

`tools/testing/test-native-linux.py` bounds game runs to600s, locks
`private/test-saves/native-linux`, isolates all input/display/save/XDG/crash paths,
and skips startup movies like the routine Windows runner. `--capture` retains two
rotating BMPs under `private/captures/native-linux`; `--test-keys` consumes the
existing synthetic SDL event format. Test scripts never use personal configs/saves.
Successful presents and captures are separate from visually reviewed gameplay.
Patch2412 enables the shared XMV/FFmpeg player on native Vulkan hosts. Routine
runs skip startup movies; use --movies for separate movie acceptance.

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
System llvmpipe remains available as a software baseline. Direct delivery and
RTX acceleration have separate acceptance evidence. Keep Original 60 FPS selected.

The isolated Mesa 26.2.5 Dozen driver exposes the RTX 4070 SUPER with Vulkan 1.2,
without maintenance4. The compatibility path below now passes renderer/replay
and actual first-level gameplay acceptance. System Mesa/project pins are unchanged.
The manual profile's 240 FPS beta choice was backed up and reset to Original 60 FPS.

Vulkan1.2 compatibility is implemented in final patch2407. Devices must
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

Hardware-driver profiling exposed repeated text `/proc/self/maps` scans in
`pc_guest_query_region` as a CPU bottleneck. Linux now tries the kernel 6.11+
PROCMAP_QUERY binary ioctl (fixed 104-byte UAPI, without relying on new headers),
then falls back to text only for an unsupported ioctl. Holes/permission failures
remain denied; every access re-queries live protection boundaries. No snapshots
persist across guest execution. The controlled `test_linux_map_query.py` fixture
checks permissions, changing protection, invalid boundaries, holes and fallback.
`tools/play-native-linux.py --game-dir PATH --wsl-gpu` verifies the private driver
receipt and opens direct Wayland on WSLg with persistent isolated manual saves,
shader caching and bounded logs. Validation is opt-in for interactive performance;
the bounded acceptance runners keep validation enabled.

The WSL Dozen ICD selection also disables glibc's ERMS/FSRM memcpy dispatch for
that child process only, preserving other tunables. Profiling found REP MOVSB
dominating copies to D3D12 upload memory; this selects the existing vector-copy
implementation. Ordinary native Linux/system/software drivers retain their
environment. Tests cover this scope and preservation; see
[WSL GPU development](../development/wsl-vulkan.md) for build/play commands.

The bounded game runner's explicit `--performance` uses interactive cache/on and
validation/off settings and records both in its receipt. It is performance
evidence, not validation acceptance; the default runner remains validation/on.
Linux CI also runs the shared RHI/replay with explicit `--compat12`; separate
receipt/log suffixes retain both normal and compatibility evidence.

## Linux host optimization and controlled refresh tests

`build-native-linux.py` now applies `-O2 -g` to C/C++ host libraries in the
existing Debug configuration. The title retains its explicit `-O2
-fno-vectorize -fno-slp-vectorize` numeric policy; assertions, symbols and console
interrupt timing remain. `--host-optimization debug` restores `-g` for host
libraries in the same build tree; both flags are passed explicitly to replace
cached CMake settings. BUILD.json records the selected host profile/flags.
Windows/Mac build options and their existing runtime branches are unchanged.

The bounded runner offers `--fps 60|120|180|240`, with 60 as the default.
It writes only its disposable adjacent pc-frame-rate.ini, refusing file links,
because saved game-menu preferences take precedence over TSFP_NATIVE_FPS.
A stale 240 preference previously invalidated attempted 60-FPS comparisons.
Receipts record the requested/observed startup mode and child user/system CPU
seconds; a mismatched startup mode fails the run. High-refresh gameplay remains
the existing beta feature; setting a target does not prove it is sustained.
The Linux runner regressions check all four presets, invalid values and linked
file preservation. Original-code native timing fixtures remain shared.

Final platform patch2409 enables the existing Windows/Mac late-frame vblank
observation on Linux. The missing host guard made all six positive native
`test_frame_clock.py` deadline cases fail on Linux: a frame already late enough
to consume a real signaled vblank instead waited another tick. High-refresh
frames still bypass the original gate, and their return-to-original handoff
retains the last real vblank. The existing missing-observer mutation still must
fail. Cheap frames and absent signals continue waiting; the console ISR stays
at 60 Hz. This is the shared pacing rule, not a Linux-specific timing algorithm.

Linux CI now runs the existing frame-clock deadline/preset fixtures after the
RHI step prepares and patches the runtime; these fixtures must exercise the
Linux branch, not only Windows/Mac host builds.

## Vulkan upload batching and benchmark evidence

Final shared Vulkan patch2411 uses `src/graphics/upload_copy.h` to batch tight
rows/slices while preserving padded/short-row bytes. Uniform buffer updates
compare exact CPU-shadow bytes (including signed zero/NaN payloads); unchanged
updates retain the existing dirty state/snapshot instead of forcing a new GPU
copy. Direct map/unmap still marks dirty; snapshot creation compares against an
optional CPU mirror of the last snapshot before allocating again. Reuse requires
the same frame, sufficient range and exact bytes. No GPU/upload-memory reads
are introduced. Allocation failure retains the original upload behavior, and
frame/range changes still resnapshot.
All Vulkan hosts share this implementation; D3D11 is unchanged. The real GPU
512-draw/six-arena fixture repeats both dirty and clean identical updates.
The byte-copy fixture checks tight/padded/short/compressed layouts, slice gaps,
sentinels and uniform identity under optimized/sanitized builds.

Opt-in `TSFP_RHI_UPLOAD_PROFILE=1` logs cumulative image bytes/copy calls,
uniform updates/skips, snapshot checks/reuses, GPU uniform bytes, wait API calls/time and pipeline
creation calls each300 frames. Counters describe host work/API wait duration,
not GPU completion. Default runs perform no profiling clock queries or logs.
`TSFP_RHI_UPLOAD_BASELINE=1` selects legacy row copies/dirty updates for a
same-binary comparison. Runner `--upload-profile`/`--upload-baseline` select
these diagnostics explicitly; inherited overrides are still removed.

The runner's `--profile-name` keeps named fixture copies separate and reserves
manual/XDG/crash folders. `--frame-profile --profile-start SECONDS` records
bounded present intervals and percentiles using the shared analysis helper.
`--input-script tests/fixtures/linux-story-benchmark.json` provides bounded
synthetic SDL navigation into Story2401 Normal using the existing Test Lab
profile. Script size/event count/time ordering/command grammar are validated
before a game starts. Captures must establish the actual measured scene;
a successful script alone is not gameplay evidence. The script uses S for
fixture menu movement. `--story-benchmark` reads only its owned child's supported-US
scene/cinematic/pause words through read-only `/proc/PID/mem`. Opening cinematic
scene24 is skipped after50seconds plus two seconds of observed cinematic state;
one Continue tap handles a resulting pause. Active Story requires scene2,
no cinematic bit and no pause. Missing/short active windows fail the run.
Frame timestamps share the monotonic clock; summaries exclude five seconds of
active warmup and clip to CSV coverage. Compare equal simulation-phase windows,
not merely equal wall-clock offsets. Disable captures for performance runs;
verify pixels separately with captures and validation enabled.

Linux cleanup now passes the active RECOMP_SAVE_DIR into the shared recycler;
only its direct Partition0..5 images move to Trash. Manual and sibling test
profiles remain untouched. Existing Mac recycler callers retain their scope.

## Cross-platform buffer contract

The shared `tests/support/rhi_compatibility.h` fixture uses default uniform
buffers for update() and a separate dynamic CPU-write uniform buffer for discard
map/unmap. Both paths render repeated identical constants and changed values
across512 draws and six arena reuses. Do not map a default buffer: Vulkan's CPU
shadow can accept it, but D3D11 correctly rejects discard mapping without dynamic
usage/CPU-write access. Rebind the default buffer before cleanup so subsequent
pixel tests retain their original state. This same fixture runs on Windows WARP,
Mac Vulkan and Linux Vulkan; no host-specific test exclusion is needed.

Linux GCC also compiles shared Story cinematic/menu contracts with fatal warnings.
Keep the zero-serial skip and subsequent assignment on separate statements/lines;
this is an unconditional assignment after the skip, not part of the if body.
Demo mutation fixtures accept both glibc and Windows/macOS assertion prefixes
while requiring the specific broken expression; assertion failures remain required.

Linux runtime CI now runs the same shared timing/vehicle/demo/wind, mouse/reload,
HUD/FOV/cutscene/menu and audio contracts after applying the runtime. Asset-free
cases execute on CI; original-routine cases additionally require the local US XBE.
Windows/macOS native jobs retain their existing suites and the shared GPU fixture.

## Desktop and standalone delivery

Final additive patch2412 enables the existing XMV player on POSIX through the
host clock/path/audio adapters and requires Linux FFmpeg headers via pkg-config.
GTK3 is the Linux desktop widget adapter: settings_linux.c stages PcSettings,
uses shared conflict/validation/atomic persistence, and applies the shared display
model. Debug controls enumerate pc_debug_options.def, preserve unsupported
capabilities and poll bounded shared snapshots. Windows/Cocoa retain their UIs.
Linux Online accepts explicit interpreter/script/manifest/handshake environment
paths for relocated packages; checkout defaults remain. Never package developer
venvs, game assets or personal settings. Native UI/runtime validation is separate
from source/portable fixture checks.

`tools/release/package-native-linux.py` assembles an asset-free Linux preview
with the existing self-contained launcher, ELF dependency closure excluding
host glibc, a relocated Python/Tk/TLS Online runtime and checksummed source/notices.
It verifies the native source/binary receipt, preserves existing outputs, rejects
protected/linked payloads, and checks its archive with the actual launcher.
`linux-launch.sh --game` supports an external data path and XDG writable state
for read-only installs; default launch opens the portable launcher in a writable
install. System Vulkan ICDs are not copied. A clean-machine pass and dependency
source/license audit remain separate publication requirements.

The bounded runner's `--package-root` verifies the package ELF hash, uses its
local RPATH/FFmpeg/Online files, and removes checkout SDK/loader overrides. The
same owned-child Story observation can therefore validate a relocated binary.

`--movies` enables real startup XMV playback for separate movie/overlay/audio
acceptance; routine Story benchmarks still skip it. Linux binding changes are
currently available through dropdowns; capture-to-bind self-test remains skip77.

Linux CI compiles the actual GTK adapter against pinned SDL and system GTK headers
with implicit API declarations fatal, after runtime preparation; original movie
fixtures also run there when local movie data is available.

POSIX movie playback is enabled only with TSFP_RHI_RENDERER, matching its video
library link. Legacy Mac GL keeps its prior movie stub; shared native Vulkan
hosts compile the player. This avoids introducing RHI symbols into legacy GL.

GTK initialization preserves the game's process-wide locale, and sensitivity
fields format/parse ASCII numbers. Shared storage snapshots and device retry
unlocking retain their original control flow with unambiguous statement breaks
so GCC fatal-warning fixtures compile; the callback fixture does the same.

Runner receipts distinguish validation_requested from validation_enabled; the
latter requires the RHI's actual activation log. An unavailable optional SDK
validation layer in an ordinary package must not be reported as API validation.

`--resources` samples only the owned child's RSS/address-space bytes, thread and
file-descriptor counts every five seconds, with at most128 scalar samples. It
reads no environment, unrelated process or memory contents. Compare stable
active phases; allocation/cache warmup is not by itself evidence of a leak.
