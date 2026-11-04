# WSL hardware Vulkan development

The Linux game uses the same gameplay, input and Vulkan frontend as Windows and
Mac. WSL's Ubuntu system Vulkan driver on the development host is llvmpipe
(software rendering). The isolated Mesa Dozen driver routes Vulkan through the
Windows D3D12 bridge to the RTX 4070 SUPER. It remains an experimental driver,
separate from a conformant native Linux hardware driver and Steam Deck acceptance.
Do not replace Ubuntu's system Mesa or raise the driver's advertised API version.

## Build and select the private driver

Work in the native Linux checkout, synchronized through Git with the Windows
editing checkout. Keep the existing game extract; pass its location explicitly.
The shared Python environment, SDK and game build instructions are in
[Linux development](linux.md).

The driver build needs Clang, Ninja, pkg-config, Bison, Flex, python3-mako,
python3-yaml, zlib/expat/zstd development headers, and X11/XCB/Wayland development
headers. Mesa's checked wrap supplies DirectX-Headers when the distro headers
are too old. Build without installing any driver into `/usr`:

```sh
CC=clang CXX=clang++ .venv-linux/bin/python tools/build/build-wsl-vulkan.py
ICD="$PWD/external/mesa-dzn/install/share/vulkan/icd.d/dzn_icd.x86_64.json"
.venv-linux/bin/python tools/testing/test-rhi-linux.py --driver-manifest "$ICD"
.venv-linux/bin/python tools/testing/test-native-linux.py \
  --game-dir /path/to/existing/game --driver-manifest "$ICD" \
  --seconds 120 --capture --unlocked-profile
```

The helper pins Mesa 26.2.5 to its publisher SHA-256, keeps Meson 1.9.2 private,
and records the archive, source revision, local patch, installed library and ICD
hashes in ignored `external/mesa-dzn/SOURCE.json`. Its small
`mesa-dzn-granularity.diff` corrects graphics/compute queue transfer granularity
to `(1,1,1)`, as required for those queues and supported by the driver's region
copy implementation. No system drivers or project SDK pins change.

The Linux test runners explicitly select the ICD and supply `/usr/lib/wsl/lib`
when the installed D3D12 bridge exists. They keep Vulkan validation enabled.
On a root WSL session, use the actual WSLg runtime directory if the inherited
XDG directory does not contain its Wayland socket:

```sh
export SDL_VIDEODRIVER=wayland XDG_RUNTIME_DIR=/mnt/wslg/runtime-dir
```

## Interactive development play

After renderer and actual-game acceptance, open the development executable:

```sh
.venv-linux/bin/python tools/play-native-linux.py \
  --game-dir /path/to/existing/game --wsl-gpu
```

This verifies the executable and private driver receipts, opens a direct SDL
Wayland window under WSLg, and retains the manual Test Lab profile under
`private/test-saves/native-linux/manual`. It shares the test runner's session
lock. Personal game saves are not selected. Ordinary native Linux systems can
omit `--wsl-gpu` to use their installed Vulkan driver or select an explicit ICD.
The interactive route enables the isolated shader cache and bounded log drains;
add `--validation` for debugging. Mouse capture uses the common SDL implementation.

For the explicit Dozen ICD under WSL, driver selection disables glibc's ERMS/FSRM
memcpy dispatch in that child environment. Profiling found REP MOVSB dominating
copies into D3D12 upload memory; the ordinary vector-copy implementation avoids
that path. Other tunables remain, and system/native Linux drivers are unaffected.
This is a process-local glibc dispatch choice, not an OS or CPU setting. The
[glibc tunable documentation](https://sourceware.org/glibc/manual/2.39/html_node/Hardware-Capability-Tunables.html)
describes the supported override mechanism.

## Shared compatibility and performance contracts

Final patches2407/2408 allow Vulkan 1.2 only with dynamic rendering,
synchronization2 and timeline semaphore support. Missing push descriptors and
extended dynamic state use descriptor arenas and baked pipeline state. Without
maintenance4, the SPIR-V adapter widens fragment interfaces and extracts their
original lanes, preserving shader arithmetic. Vulkan 1.3 retains its existing
path; `RECOMP_VK_COMPAT12=1` forces compatibility for regression testing.

The common RHI tests cover all varying widths, 512 independent descriptor draws
without intermediate flush, repeated arena reuse, textures, screen copies,
depth/stencil, blend/scissor state, readback and presentation. They run on RTX
Dozen, llvmpipe's existing path and forced compatibility; Windows uses the same
pixel contract through WARP. Mac hardware acceptance is recorded separately.

Profiling the actual game with Dozen exposed repeated text memory-map scans as
a CPU bottleneck. `pc_guest_query_region` uses the Linux 6.11+ binary
PROCMAP_QUERY ioctl, with the old text parser for unsupported kernels. Each call
queries current protection; no mapping cache survives guest execution. Tests
cover changed permissions, protected pages, holes and unsupported-kernel fallback.
See the [kernel documentation](https://www.kernel.org/doc/html/latest/filesystems/proc.html)
for the binary query and [Vulkan queue requirements](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#devsandqueues-physical-device-enumeration)
for transfer granularity.

Build success and passing asset-free tests are separate from actual gameplay
and measured performance. Keep run revision, driver, validation setting,
resolution, scene and limitations in STATUS; do not infer Deck or full-game parity.
