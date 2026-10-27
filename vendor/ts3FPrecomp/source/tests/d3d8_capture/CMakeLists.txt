# d3d8_capture -- round-trip the frame capture container.
#
# Builds the real d3d8_capture.c. It carries no Direct3D, Windows or guest
# memory dependency, so unlike tests/nv2a_vsh_hlsl this runs on Linux as well
# as Windows, and the Linux CI job covers the format. No device, window or
# game files -- the capture it reads is the synthetic one it just wrote.

add_executable(d3d8_capture_test
    test_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hle/d3d8_capture.c
)
target_include_directories(d3d8_capture_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hle
)

add_test(NAME d3d8_capture COMMAND d3d8_capture_test)
