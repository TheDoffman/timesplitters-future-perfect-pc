# nv2a_vsh_hlsl -- compile the HLSL the vertex-program translator generates.
#
# Windows only: it needs D3DCompile. Builds d3d8_vsh.c and nv2a_vsh.c with
# stubs for the four device accessors d3d8_vsh.c links against, the same way
# tests/d3d8_smoke builds its targets. No device, window or game files.

add_executable(nv2a_vsh_hlsl_test
    test_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/d3d/d3d8_vsh.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/kernel/nv2a_vsh.c
    # d3d8_vsh.c creates its objects through rhi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/d3d/rhi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/d3d/rhi_d3d11.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/d3d/rhi_shader_cache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/d3d/d3d8_compile.c
)
target_include_directories(nv2a_vsh_hlsl_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/d3d
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/kernel
)
# xbox_config: d3d8_vsh.c reads its settings through recomp_config.h, and
# without it this test has not compiled since that include arrived.
target_link_libraries(nv2a_vsh_hlsl_test PRIVATE
    platform d3d11 dxgi dxguid d3dcompiler xbox_config
)

add_test(NAME nv2a_vsh_hlsl COMMAND nv2a_vsh_hlsl_test)
