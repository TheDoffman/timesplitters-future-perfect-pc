# The settings file, parsed without a game: pure text in, values out.

add_executable(config_smoke test_config.c)
target_link_libraries(config_smoke PRIVATE xbox_config)
if(MSVC)
    target_compile_options(config_smoke PRIVATE /W4)
else()
    target_compile_options(config_smoke PRIVATE -Wall -Wextra)
endif()

# The input config the launcher writes. Given the runtime's own source as
# an argument so the vocabulary is checked against it rather than against
# a copy of it -- a control name the runtime does not know is a binding
# that silently does nothing.
if(WIN32)
    add_executable(bindings_smoke
        test_bindings.c
        ${CMAKE_CURRENT_LIST_DIR}/../../src/launcher/launcher_bindings.c)
    target_include_directories(bindings_smoke PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../src/launcher)
    if(MSVC)
        target_compile_options(bindings_smoke PRIVATE /W4)
    endif()
endif()
