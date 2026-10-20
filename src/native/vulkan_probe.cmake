# Asset-free contract for the production common RHI backend.
set(TSFP_VULKAN_SDK "" CACHE PATH "Pinned local macOS Vulkan SDK (macOS directory)")
if(TSFP_VULKAN_SDK)
    if(NOT TSFP_RUNTIME_DIR)
        get_filename_component(TSFP_RUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    endif()
    set(TSFP_VK_SOURCE "${TSFP_RUNTIME_DIR}/src/d3d")
    add_executable(tsfp_rhi_vulkan_test
        "${TSFP_NATIVE_DIR}/../../tests/rhi_menu_sampling.c"
        "${TSFP_VK_SOURCE}/rhi.c" "${TSFP_VK_SOURCE}/rhi_shader_cache.c"
        "${TSFP_VK_SOURCE}/d3d8_compile.c" "${TSFP_VK_SOURCE}/d3d8_resources.c"
        "${TSFP_VK_SOURCE}/d3d8_screencopy.c")
    target_include_directories(tsfp_rhi_vulkan_test PRIVATE "${TSFP_VK_SOURCE}"
        "${TSFP_RUNTIME_DIR}/src" "${TSFP_RUNTIME_DIR}/src/config" "${TSFP_NATIVE_DIR}/.." "${TSFP_NATIVE_DIR}/../../tests")
    target_link_libraries(tsfp_rhi_vulkan_test PRIVATE platform)
    target_compile_options(tsfp_rhi_vulkan_test PRIVATE -O2)
    include("${TSFP_NATIVE_DIR}/vulkan_backend.cmake")
    tsfp_add_vulkan_backend(tsfp_rhi_vulkan_test "${TSFP_RUNTIME_DIR}" "${TSFP_VULKAN_SDK}")
endif()
