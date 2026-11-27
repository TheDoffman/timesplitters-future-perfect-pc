# Cross-compiles the native Linux game for aarch64 on an x86_64 host.
# The sysroot is Valve's Steam Runtime 4 arm64 SDK sysroot, so the result links
# against the same glibc and libraries the game runs on.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(TSFP_SYSROOT "$ENV{TSFP_ARM64_SYSROOT}" CACHE PATH "Steam Runtime 4 arm64 SDK sysroot")
if(NOT TSFP_SYSROOT)
    message(FATAL_ERROR "Set TSFP_ARM64_SYSROOT to the extracted arm64 SDK sysroot")
endif()
set(CMAKE_SYSROOT "${TSFP_SYSROOT}")

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET aarch64-linux-gnu)

# lld links without needing the arm64 binutils; libgcc and the startup files
# come from the sysroot's gcc 14 installation.
set(TSFP_GCC_LIB "${TSFP_SYSROOT}/usr/lib/gcc/aarch64-linux-gnu/14")
set(CMAKE_C_FLAGS_INIT "--sysroot=${TSFP_SYSROOT} --gcc-toolchain=${TSFP_SYSROOT}/usr -fuse-ld=lld -B${TSFP_GCC_LIB}")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_C_FLAGS_INIT}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld -B${TSFP_GCC_LIB}")

set(CMAKE_FIND_ROOT_PATH "${TSFP_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
