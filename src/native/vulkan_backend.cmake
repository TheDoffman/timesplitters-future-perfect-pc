# Reuse the same production Vulkan backend for the standalone contract and game.
function(tsfp_add_vulkan_backend target runtime sdk)
    get_filename_component(runtime "${runtime}" ABSOLUTE)
    if(NOT sdk OR NOT EXISTS "${sdk}/include/dxc/dxcapi.h")
        message(FATAL_ERROR "Verify the pinned host Vulkan SDK with the project's RHI test runner first")
    endif()
    set(source "${runtime}/src/d3d")
    set(third "${runtime}/third_party")
    foreach(required Vulkan-Headers/include/vulkan/vulkan.h volk/volk.c VulkanMemoryAllocator/include/vk_mem_alloc.h)
        if(NOT EXISTS "${third}/${required}")
            message(FATAL_ERROR "Initialize pinned Vulkan submodules: ${required}")
        endif()
    endforeach()
    target_sources(${target} PRIVATE "${source}/rhi_vulkan.c" "${source}/rhi_vulkan_dxc.cpp"
        "${source}/rhi_vulkan_vma.cpp" "${third}/volk/volk.c")
    target_include_directories(${target} PRIVATE "${third}/Vulkan-Headers/include"
        "${third}/volk" "${third}/VulkanMemoryAllocator/include" "${sdk}/include/dxc")
    if(APPLE)
        set(dxc "${sdk}/lib/libdxcompiler.dylib")
        target_link_libraries(${target} PUBLIC c++)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(dxc "${sdk}/lib/libdxcompiler.so")
    else()
        message(FATAL_ERROR "The SDL Vulkan host supports macOS and Linux")
    endif()
    if(NOT EXISTS "${dxc}")
        message(FATAL_ERROR "Missing host DXC library: ${dxc}")
    endif()
    target_compile_definitions(${target} PRIVATE RHI_HAVE_VULKAN RHI_VULKAN_SDL VK_NO_PROTOTYPES
        "RHI_DXC_LIBRARY=\"${dxc}\"")
    target_link_libraries(${target} PUBLIC SDL3::SDL3 ${CMAKE_DL_LIBS})
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # VMA annotates some, but not all, pointers. Keep this vendored-only
        # diagnostic policy identical for Apple Clang and Linux Clang.
        set_source_files_properties("${source}/rhi_vulkan_vma.cpp" PROPERTIES
            COMPILE_OPTIONS "-Wno-nullability-completeness")
    endif()
    # C entry points consume the C++ DXC/VMA implementation on both hosts.
    set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CXX)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 17)
endfunction()
