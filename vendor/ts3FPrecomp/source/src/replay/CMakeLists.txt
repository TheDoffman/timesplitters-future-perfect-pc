# d3d8_replay -- replay a captured frame into the host renderer.
#
# Windows only: the D3D11 backend is (src/d3d/CMakeLists.txt builds d3d8_gl.c
# on POSIX instead, which has no equivalent entry points for this tool).
#
# It compiles d3d8_capture.c directly rather than linking xbox_hle, because
# the container has no dependencies and xbox_hle would drag in the kernel
# layer, XAudio2 and XInput for a tool that needs none of them.

# nv2a_vsh.c is built in rather than linked: xbox_d3d8's vertex program
# translator calls the microcode decoder, which lives in src/kernel and is
# otherwise only reachable through xbox_kernel. tests/nv2a_vsh_hlsl compiles
# it the same way, for the same reason -- neither wants the kernel layer.
add_executable(d3d8_replay
    d3d8_replay.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../hle/d3d8_capture.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../kernel/nv2a_vsh.c
)
target_include_directories(d3d8_replay PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../hle
    ${CMAKE_CURRENT_SOURCE_DIR}/../kernel
)
target_link_libraries(d3d8_replay PRIVATE xbox_d3d8 user32)
