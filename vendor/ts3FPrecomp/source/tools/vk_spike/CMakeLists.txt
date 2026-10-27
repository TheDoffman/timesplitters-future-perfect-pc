# vk_spike -- a standalone Vulkan probe for docs/technical/vulkan-backend.md.
# Not part of the toolkit build and not linked by any title. Needs the Vulkan
# SDK (find_package(Vulkan)) for the headers, the loader import library and
# the validation layer it runs under.
#
#   cmake -S tools/vk_spike -B tools/vk_spike/build -G "Visual Studio 16 2019" -A x64
#   cmake --build tools/vk_spike/build --config Release

cmake_minimum_required(VERSION 3.16)
project(vk_spike C)

find_package(Vulkan REQUIRED)
add_executable(vk_spike vk_spike.c)
target_link_libraries(vk_spike PRIVATE Vulkan::Vulkan)
if(MSVC)
    target_compile_options(vk_spike PRIVATE /W4)
    target_compile_definitions(vk_spike PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
