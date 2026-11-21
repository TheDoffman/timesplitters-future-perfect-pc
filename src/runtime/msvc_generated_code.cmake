# MSVC-only host compiler policy, not a generated guest-code modification.
function(tsfp_configure_generated_msvc target generated_dir)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    get_filename_component(project_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    set(TSFP_MSVC_GENERATED_DIR "${generated_dir}")
    set(policy "${CMAKE_BINARY_DIR}/generated/msvc-generated-code.cmake")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${project_root}/tools/build/msvc_generated_code.py"
        --generated-dir "${generated_dir}" --output "${policy}"
        RESULT_VARIABLE result OUTPUT_VARIABLE report ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "MSVC generated-code compatibility policy failed: ${report}${error}")
    endif()
    message(STATUS "${report}")
    include("${policy}")
    set_target_properties(${target} PROPERTIES
        VS_GLOBAL_UseMultiToolTask true VS_GLOBAL_EnforceProcessCountAcrossBuilds true)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${project_root}/config/build/msvc-generated-code.json"
        "${project_root}/tools/build/msvc_generated_code.py")
endfunction()
