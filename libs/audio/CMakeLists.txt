# libs/audio — sample ring, audio clock and outputs (PipeWire, null). Depends on base only.
# PipeWire/SPA headers are C with heavy macros: include them as system headers.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
oma_library(audio SOURCES ${SOURCES} DEPS base)
target_include_directories(oma_audio SYSTEM PRIVATE ${PIPEWIRE_INCLUDE_DIRS})
target_link_libraries(oma_audio PRIVATE ${PIPEWIRE_LIBRARIES})
