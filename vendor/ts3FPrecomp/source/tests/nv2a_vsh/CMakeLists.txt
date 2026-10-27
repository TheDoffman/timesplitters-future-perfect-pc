# nv2a_vsh -- runs the real NV2A vertex-program decoder and interpreter.
#
# Compiles src/kernel/nv2a_vsh.c on its own. It is pure C over 32-bit words,
# so the test needs no graphics API, no kernel library and no game files, and
# runs on every platform CI builds.

add_executable(nv2a_vsh_test
    test_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/kernel/nv2a_vsh.c
)
target_include_directories(nv2a_vsh_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/kernel
)
if(NOT MSVC)
    target_link_libraries(nv2a_vsh_test PRIVATE m)
endif()

add_test(NAME nv2a_vsh COMMAND nv2a_vsh_test)
