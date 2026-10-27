cmake_minimum_required(VERSION 3.20)

# Xbox Kernel Replacement Layer
# Replaces Xbox kernel imports with Win32 equivalents.
# Memory layout, file I/O, threading, synchronization, crypto, HAL.

set(KERNEL_SOURCES
    kernel_path.c
    kernel_pool.c
    kernel_rtl.c
    kernel_memory.c
    kernel_file.c
    kernel_thread.c
    kernel_sync.c
    kernel_hal.c
    kernel_xbox.c
    kernel_ob.c
    kernel_io.c
    kernel_crypto.c
    kernel_thunks.c
    kernel_bridge.c
    xbox_memory_layout.c
    xbox_watchpoint.c
    icall_feedback.c
    exit_trace.c
    recomp_trace.c
    recomp_fps.c
    recomp_sample.c
    nv2a_pb_scan.c
    nv2a_pb_exec.c
    nv2a_vsh.c
)

add_library(xbox_kernel STATIC ${KERNEL_SOURCES})

# See src/platform/CMakeLists.txt: resolve src/ relative to this file, not via
# CMAKE_SOURCE_DIR, which is the consuming project's root under add_subdirectory().
get_filename_component(XBOXRECOMP_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

target_include_directories(xbox_kernel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${XBOXRECOMP_SRC_DIR}
)
target_link_libraries(xbox_kernel PUBLIC xbox_config)

if(WIN32)
    # platform is an INTERFACE target on Windows: it contributes the src/
    # include path that "platform/xbox_winnt.h" resolves against.
    # winmm for timeBeginPeriod (the vblank clock's fallback timer), dbghelp
    # and psapi for the sampling profiler's symbol and module lookups.
    target_link_libraries(xbox_kernel PUBLIC platform kernel32 bcrypt winmm dbghelp psapi)
else()
    # Linux: platform layer (POSIX host primitives) + OpenSSL for crypto.
    find_package(OpenSSL REQUIRED)
    target_link_libraries(xbox_kernel PUBLIC platform OpenSSL::Crypto)
endif()

if(MSVC)
    # /Zi in every configuration: the title links with /DEBUG, so the PDB then
    # names this library's static functions too, and a sampled profile
    # (RECOMP_SAMPLE) attributes time to nv2a_ack_thread rather than to whatever
    # public symbol happens to precede it. No effect on the generated code.
    target_compile_options(xbox_kernel PRIVATE /W4 /WX- $<$<NOT:$<CONFIG:Debug>>:/O2> /GS- /Gy /Zi)
    target_compile_definitions(xbox_kernel PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(xbox_kernel PRIVATE -Wall -Wextra -Wno-unused-parameter -O2)
endif()

target_compile_definitions(xbox_kernel PRIVATE $<$<CONFIG:Debug>:_DEBUG>)
