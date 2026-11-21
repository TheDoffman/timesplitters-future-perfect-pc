# Windows Vulkan default and MSVC build policy

Normal Windows packages build both backends and prefer Vulkan. Patch
`graphics/rendering/3050-windows-vulkan-primary.patch` changes initial RHI
selection while keeping D3D11 first in the recovery table. An explicit
`RECOMP_D3D8_BACKEND=d3d11` remains available. Linux/native Mac already prefer
Vulkan; legacy Mac OpenGL is unchanged.

Before Vulkan allocates GPU resources, `rhi_vk_dxc_available` initializes the
verified dynamic shader compiler. Missing DXC returns through the same D3D11
recovery path as a missing Vulkan driver. Partial initialization also destroys
the instance/surface/debug messenger, and allocator destruction is guarded.
`rhi_d3d11_adopt` explicitly selects the backend owning the adopted device;
WARP fixtures must not keep the initial Vulkan dispatch table.

`tests/windows-rhi` and `tools/testing/test-rhi-windows.py` exercise default
Vulkan, explicit D3D11/Vulkan, missing driver/compiler/device recovery and
explicit D3D11 adoption. The no-device case uses the loader's vendor filter in
the child process only. With a verified SDK path, ordinary Vulkan cases require
active validation and reject any Vulkan diagnostic or silent fallback.
`native_refresh_game.py --backend auto` leaves the backend override absent and
requires actual Vulkan guest presents. `--backend d3d11` provides the reference.
Tagged JSON reports record requested and observed backend; saves/config/cache
paths remain isolated. Frame profiling and capture/validation are separate runs.

The normal Release build applies maintained MSVC compatibility rules through
`src/runtime/msvc_generated_code.cmake` and `tools/build/msvc_generated_code.py`.
`config/build/msvc-generated-code.json` identifies the eight measured compiler
exceptions by guest definition, not generated chunk filename. Three units use
`/Od /Ob0`; the others limit inlining with `/Ob1`. Other source keeps Release
optimization. Debug is unchanged. This is a supported host-compiler policy,
not a generated C edit or a change to guest instructions. It preserves bounded
builds after source regrouping and fails on missing/duplicate policy definitions.
The output JSON records affected units and symbols.

MSBuild uses fresh per-source compiler processes, with `-CompileJobs` controlling
their bound (default two). Tests cover regrouped chunks, unchanged source bytes,
stronger rules when definitions share a unit, invalid/absent definitions,
and CMake's actual Debug/Release option expansion. Do not restore handwritten
VCXPROJ exceptions or overwrite another checkout's pending compiler work.

Standard build replay checks explicitly use D3D11, so building does not require
a validation SDK. Dedicated Vulkan acceptance uses the verified SDK. Release
packaging lists Vulkan as the default and includes the pinned DXC DLLs/notices.
The historical controls-upgrade fixture defers patch 3050 until its full graphics
prerequisites are restored, then verifies that the upgrade installs the Vulkan
preference, compiler preflight and maintained MSVC policy.
See [player/build commands](../players/windows-vulkan.md) and measured results
in [STATUS](../project/STATUS.md).
