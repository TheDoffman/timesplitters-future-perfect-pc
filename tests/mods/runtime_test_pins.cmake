# Generated only for the native factory fixture; production pins use the builder.
file(SHA256 "${WORKER}" worker_hash)
set(guardian_hash "")
if(GUARDIAN)
    file(SHA256 "${GUARDIAN}" guardian_hash)
endif()
get_filename_component(directory "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${directory}")
file(WRITE "${OUTPUT}" "#define TSFP_MOD_RUNTIME_SUBDIR \"ModRuntime/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"\n#define TSFP_MOD_RUNTIME_WORKER \"${NAME}\"\n#define TSFP_MOD_RUNTIME_WORKER_SHA256 \"${worker_hash}\"\n#define TSFP_MOD_RUNTIME_GUARDIAN_SHA256 \"${guardian_hash}\"\n")
