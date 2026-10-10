# libs/media — probe and decode on top of FFmpeg (libav* confined to src/). Depends on gpu, base.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
pkg_check_modules(FFMPEG REQUIRED
    libavformat libavcodec libavutil libswresample libswscale libavfilter)
oma_library(media SOURCES ${SOURCES} DEPS gpu base)
target_include_directories(oma_media SYSTEM PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(oma_media PRIVATE ${FFMPEG_LIBRARIES})
