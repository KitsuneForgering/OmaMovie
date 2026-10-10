# tools/oma-project — the native project inspect/validate/dump CLI (CLAUDE.md §16.7).
file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(oma-project ${SOURCES})
set_target_properties(oma-project PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools/oma-project)
target_compile_options(oma-project PRIVATE ${OMA_LIB_WARNINGS})
target_link_libraries(oma-project PRIVATE oma_project oma_timeline oma_base)
