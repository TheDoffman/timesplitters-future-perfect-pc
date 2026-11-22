cmake_minimum_required(VERSION 3.20)
project(tsfp_windows_rhi_contract C CXX)
if(NOT WIN32)
    message(FATAL_ERROR "Use the Mac/Linux RHI runner on this host")
endif()
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(runtime "${root}/external/ts3FPrecomp/src")
set(third "${root}/external/ts3FPrecomp/third_party")
add_executable(tsfp_windows_rhi_test
    "${root}/tests/graphics/rhi_menu_sampling.c"
    "${runtime}/d3d/rhi.c" "${runtime}/d3d/rhi_d3d11.c"
    "${runtime}/d3d/rhi_shader_cache.c" "${runtime}/d3d/d3d8_compile.c"
    "${runtime}/d3d/d3d8_resources.c" "${runtime}/d3d/d3d8_screencopy.c"
    "${runtime}/d3d/d3d8_states.c" "${runtime}/config/recomp_config.c"
    "${runtime}/d3d/rhi_vulkan.c" "${runtime}/d3d/rhi_vulkan_dxc.cpp"
    "${runtime}/d3d/rhi_vulkan_vma.cpp" "${third}/volk/volk.c")
target_include_directories(tsfp_windows_rhi_test PRIVATE
    "${root}/src" "${root}/tests" "${runtime}" "${runtime}/d3d" "${runtime}/config"
    "${third}/Vulkan-Headers/include" "${third}/volk"
    "${third}/VulkanMemoryAllocator/include" "${third}/dxc/inc")
foreach(group diagnostics gameplay graphics input interface network runtime storage textures)
    target_include_directories(tsfp_windows_rhi_test PRIVATE "${root}/src/${group}")
endforeach()
target_include_directories(tsfp_windows_rhi_test PRIVATE "${root}/tests/support")
target_compile_definitions(tsfp_windows_rhi_test PRIVATE TSFP_WINDOWS_RHI_TEST
    WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
    RHI_HAVE_VULKAN VK_NO_PROTOTYPES VK_USE_PLATFORM_WIN32_KHR)
set_target_properties(tsfp_windows_rhi_test PROPERTIES C_STANDARD 11 CXX_STANDARD 17)
option(TSFP_TEST_OPENGL "Include the experimental OpenGL backend in hardware contracts" OFF)
option(TSFP_TEST_OPENGL_RECOVERY "Test an accepted OpenGL recovery build (fixture only)" OFF)
if(TSFP_TEST_OPENGL_RECOVERY AND NOT TSFP_TEST_OPENGL)
    message(FATAL_ERROR "Recovery contracts require TSFP_TEST_OPENGL")
endif()
if(TSFP_TEST_OPENGL)
    include("${root}/src/graphics/opengl_backend.cmake")
    tsfp_add_opengl_backend(tsfp_windows_rhi_test "${root}/external/ts3FPrecomp")
    if(TSFP_TEST_OPENGL_RECOVERY)
        target_compile_definitions(tsfp_windows_rhi_test PRIVATE TSFP_OPENGL_RECOVERY_ACCEPTED=1)
    endif()
endif()
target_link_libraries(tsfp_windows_rhi_test PRIVATE d3d11 dxgi dxguid d3dcompiler user32 gdi32)
foreach(dll dxcompiler dxil)
    add_custom_command(TARGET tsfp_windows_rhi_test POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${third}/dxc/bin/x64/${dll}.dll" "$<TARGET_FILE_DIR:tsfp_windows_rhi_test>")
endforeach()
