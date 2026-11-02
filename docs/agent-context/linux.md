# Linux x64 SDL/Vulkan foundation

Build/test entry: `tools/testing/test-rhi-linux.py --prepare-runtime --setup-sdk`.
The existing `tests/rhi` CMake project builds the common Vulkan contract and
production replay tool in `build/linux/rhi-tests`. `--build-only` leaves GPU
acceptance explicitly unrun. Linux dependencies/environment instructions and
remaining title work are in [Linux development](../development/linux.md).

Final patch `patches/platform/2401-linux-vulkan-foundation.patch` extends the
common Xbox frontend and replay SDL guards after layout/Online patches. The
loader orders it last. `src/native/vulkan_backend.cmake` selects host DXC/library
syntax only; shaders, renderer policy and the historical patch stack stay shared.
Mac GL guards, SDK pin/driver and Cocoa adapters remain unchanged.

SDK: `config/build/vulkan-sdk-linux.json`, `tools/build/linux_vulkan_sdk.py`.
Same version as Mac; verify publisher size/SHA-256 before extraction, keep
licenses and an installed-file receipt. The Linux archive's loader is under
`x86_64/lib/VulkanLoader/lib`, not directly in `lib`. Use the system hardware
ICD or pass an explicit software-driver manifest. Isolate user configs/caches;
retain the desktop's display variables and XDG_RUNTIME_DIR. Use `.venv-linux`
on a Windows/WSL shared checkout; never reuse Windows Python/build dependencies.
Never mutate/build one ignored runtime concurrently from Windows and Linux.

Memory: `src/native/host_pages.c` parses the containing VMA from `/proc/self/maps`
without reading protected pages; a hole must fail without touching the output.
`host_memory_stats.c` uses `sysinfo` for conservative free physical RAM, excluding
reclaimable cache. Common reservation/shared-view owners are unchanged;
`native_vm_host.c` enables Linux POSIX mapping declarations under strict C11.

Tests: `tests/repository/test_linux_foundation.py` checks SDK integrity/isolation.
Controlled Linux memory branches also execute on Windows/Mac via
`tests/compiler/test_linux_host.py` (VMA permissions/holes and sysinfo units,
overflow/failure preservation). Existing real-host pages/stats/native-VM/shared-
view contracts run on Linux. CI's
`linux-foundation` job uses Ubuntu x64, Xvfb and Mesa lavapipe, and the PR gate
requires success when GPU checks are selected. These are asset-free checks;
configured CI and Windows passing tests do not establish Linux execution.

Local WSL startup requires a Windows restart after installation. No Linux game
target exists yet: Linux MMIO/fault/crash, text/settings/Online and title build
adapters remain. Preserve shared gameplay changes, the supported XBE pin,
original 60 FPS defaults and user saves/settings. Steam Deck is secondary
hardware acceptance; WSL/software Vulkan does not establish Deck performance.
