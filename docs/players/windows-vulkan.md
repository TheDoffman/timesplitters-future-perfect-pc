# Windows Vulkan renderer

Windows builds use **Vulkan by default**, sharing the Vulkan backend used by
Linux and modern native Mac builds. If Vulkan cannot initialize, Windows reports
a startup error instead of switching to Direct3D 11. Check the GPU driver and
packaged shader-compiler files when diagnosing startup failures.

An experimental OpenGL recovery backend is under development. Automatic
OpenGL recovery is disabled pending game, performance and platform acceptance;
its GPU tests alone do not establish a working fallback for players.

The launcher uses Vulkan automatically and has no renderer selector. Older
saved D3D11 choices are ignored. Linux and modern native Mac packages also use
Vulkan by default.

After building, launch normally; no renderer override is needed. To force
Direct3D 11 for troubleshooting in a developer checkout:

```powershell
$env:RECOMP_D3D8_BACKEND = 'd3d11'
.\tsfp.ps1 -Action Run
```

Remove the temporary override with `Remove-Item Env:RECOMP_D3D8_BACKEND` to use
the Vulkan preference again. This does not edit player settings.

The Vulkan loader and GPU driver come from Windows/the GPU vendor. Players
do not need the Vulkan SDK. The shader compiler DLLs are included in the
release package, with their notices and a hash receipt.

## Build and compare

Run the normal `tsfp.ps1 -Action Setup` once when upgrading an older checkout,
then `tsfp.ps1 -Action Build`. Setup initializes the pinned Vulkan header,
loader-dispatch and allocator submodules and installs the hash-pinned Microsoft
DXC from `config/build/dxc-windows.json`. Build verifies the installed files
and explicitly enables `XBOXRECOMP_VULKAN`; it does not silently ship a
Direct3D-only build. Release packaging checks that flag and includes DXC.
The normal build also applies maintained MSVC compatibility rules by guest
symbol and bounds compiler processes with `-CompileJobs` (default two). It does
not require manually edited project files. Debug retains its existing options;
see the [compiler policy and test contracts](../agent-context/windows-vulkan.md).

The asset-free Windows fixture exercises both production backends on the GPU,
including shaders, texture formats, depth, MSAA, visibility, readback and
presentation. It also checks the default preference, explicit D3D11 ownership,
and startup failure without fallback for missing Vulkan drivers, usable devices and DXC:

```powershell
.venv/Scripts/cmake.exe -S tests/windows-rhi -B build/windows/rhi-compare -G "Visual Studio 17 2022" -A x64
.venv/Scripts/cmake.exe --build build/windows/rhi-compare --config Release --parallel 4
.venv/Scripts/python.exe tools/testing/test-rhi-windows.py
```

Use `--validation-layer-path <SDK>/Bin` with a separately verified Windows
Vulkan SDK to require active validation for the Vulkan case. This isolates
implicit third-party overlays in the test child only; it does not change their
system registrations. The SDK used for local validation is 1.4.363.0, publisher
SHA-256 `94a82d378f7a5e3e54c9db7d2fb7016af136e14ac0a18dbf0f2f67a36352d141`.
It was copied into the checkout using the SDK installer's documented
`--root <path> --accept-licenses --default-answer --confirm-command install copy_only=1`
mode. See [LunarG's installation guide](https://vulkan.lunarg.com/doc/view/latest/windows/getting_started.html).

`logs/rhi-windows.json` and backend logs record the outcome. Fixture execution
time includes shader compilation and readback; it is not a gameplay benchmark.
CI builds this fixture on Windows and retains the existing D3D11 WARP pixel
tests; hosted CI does not claim hardware Vulkan acceptance.

For matched isolated first-level gameplay, use the same executable and options:

```powershell
.venv/Scripts/python.exe tests/gameplay/native_refresh_game.py --fps 60 --backend d3d11 --label renderer --stress --pause-check --seconds 120 --windowed --frame-profile
.venv/Scripts/python.exe tests/gameplay/native_refresh_game.py --fps 60 --backend auto --label renderer --stress --pause-check --seconds 120 --windowed --frame-profile
```

`--backend auto` leaves the override unset and verifies actual Vulkan guest
presents. Add `--vk-validation --validation-layer-path <SDK>/Bin` for Vulkan correctness
runs. Use separate `--capture` runs to review images; capture and validation
overhead must be excluded from performance comparisons. The existing signed
Test Lab fixture is required. Runs use disposable settings/saves, verify the US
XBE, and reject silent fallback, missing guest presents and validation errors.
Frame CSVs measure CPU intervals between successful present calls, not scanout.
Compare matching gameplay windows, excluding startup, loading and pause.

Measured outcomes and the default decision belong in [STATUS](../project/STATUS.md).

The experimental OpenGL recovery backend now passes bounded Windows gameplay,
display and stability checks in developer tests. Automatic recovery remains
disabled until the remaining acceptance checks pass; Vulkan remains the default
and the launcher has no renderer selector. Native Linux/Mac recovery is unverified.
