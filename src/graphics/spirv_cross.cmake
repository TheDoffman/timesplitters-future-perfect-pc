# One source pin and one compiler implementation for every host.
function(tsfp_add_spirv_cross target)
    get_filename_component(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${root}/tools/build/spirv_cross.py"
        RESULT_VARIABLE result OUTPUT_VARIABLE source OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Verify SPIRV-Cross before building OpenGL: ${error}")
    endif()
    if(NOT TARGET tsfp_spirv_cross)
        add_library(tsfp_spirv_cross STATIC "${source}/spirv_cross.cpp" "${source}/spirv_parser.cpp"
            "${source}/spirv_cross_parsed_ir.cpp" "${source}/spirv_cfg.cpp" "${source}/spirv_glsl.cpp")
        target_include_directories(tsfp_spirv_cross PUBLIC "${source}")
        set_target_properties(tsfp_spirv_cross PROPERTIES CXX_STANDARD 17 POSITION_INDEPENDENT_CODE ON)
    endif()
    target_link_libraries(${target} PRIVATE tsfp_spirv_cross)
endfunction()
