# libs/base — rational time, Result/Error, logging and the job system. No dependencies.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
oma_library(base SOURCES ${SOURCES} DEPS)
target_link_libraries(oma_base PUBLIC Threads::Threads)
