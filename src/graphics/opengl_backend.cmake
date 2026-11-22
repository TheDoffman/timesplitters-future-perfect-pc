# The same GPU implementation is compiled by every host; window/context calls
# live in opengl_host.c. Production acceptance remains an independent gate.
function(tsfp_add_opengl_backend target runtime)
    get_filename_component(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    target_sources(${target} PRIVATE "${root}/src/graphics/opengl_backend.c"
        "${root}/src/graphics/opengl_host.c" "${root}/src/graphics/opengl_shader.cpp")
    target_include_directories(${target} PRIVATE "${root}/src/graphics"
        "${runtime}/src/d3d" "${runtime}/third_party/SDL/include")
    target_compile_definitions(${target} PRIVATE RHI_HAVE_OPENGL)
    include("${root}/src/graphics/spirv_cross.cmake")
    tsfp_add_spirv_cross(${target})
    if(WIN32)
        target_link_libraries(${target} PRIVATE opengl32)
    else()
        target_link_libraries(${target} PRIVATE SDL3::SDL3)
        if(APPLE)
            find_library(tsfp_gl_framework OpenGL REQUIRED)
            target_link_libraries(${target} PRIVATE "${tsfp_gl_framework}")
        else()
            find_package(OpenGL REQUIRED)
            target_link_libraries(${target} PRIVATE OpenGL::GL)
        endif()
    endif()
endfunction()
