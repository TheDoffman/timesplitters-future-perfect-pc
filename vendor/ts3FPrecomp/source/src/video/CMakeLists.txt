# Shared FMV player: Media Foundation decode onto a D3D8 texture.
add_library(xbox_video STATIC
    video_player.c
    video_pump.c
    fb_present.c
    xmv_demux.c
    xmv_decode.c
)
target_include_directories(xbox_video PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(xbox_video PUBLIC xbox_d3d8)
if(WIN32)
    target_link_libraries(xbox_video PUBLIC mfplat mfreadwrite mfuuid mf ole32)
endif()

# XMV movies decode through FFmpeg, loaded at run time (xmv_decode.c): only the
# headers are needed here. Without them the movies are skipped as before.
# third_party/ffmpeg is the BtbN LGPL shared build, fetched rather than
# committed -- see docs/technical/video-playback.md.
set(XBOXRECOMP_FFMPEG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/ffmpeg"
    CACHE PATH "FFmpeg shared build: include/ at build time, bin/ at run time")
get_filename_component(XBOXRECOMP_FFMPEG_DIR "${XBOXRECOMP_FFMPEG_DIR}" ABSOLUTE)
if(EXISTS "${XBOXRECOMP_FFMPEG_DIR}/include/libavcodec/avcodec.h")
    target_include_directories(xbox_video PRIVATE "${XBOXRECOMP_FFMPEG_DIR}/include")
    target_compile_definitions(xbox_video PRIVATE XMV_HAVE_FFMPEG
        XMV_FFMPEG_BIN="${XBOXRECOMP_FFMPEG_DIR}/bin")
    message(STATUS "xbox_video: XMV movies through FFmpeg at ${XBOXRECOMP_FFMPEG_DIR}")
else()
    message(STATUS "xbox_video: no FFmpeg headers at ${XBOXRECOMP_FFMPEG_DIR}; XMV movies are skipped")
endif()
