# --- developer targets (format, tidy, compdb) ---
file(GLOB_RECURSE OMA_FORMAT_FILES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/libs/*.cpp ${CMAKE_SOURCE_DIR}/libs/*.hpp
    ${CMAKE_SOURCE_DIR}/tests/*.cpp ${CMAKE_SOURCE_DIR}/tests/*.hpp)
file(GLOB_RECURSE OMA_TIDY_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/*.cpp)

add_custom_target(format
    COMMAND clang-format -i ${OMA_FORMAT_FILES}
    COMMENT "clang-format")

add_custom_target(format-check
    COMMAND clang-format --dry-run --Werror ${OMA_FORMAT_FILES}
    COMMENT "clang-format --dry-run")

# Generated SPIR-V includes must exist before clang-tidy parses vulkan_compositor.cpp.
if(TARGET oma_compositor_shaders)
    add_custom_target(tidy
        COMMAND clang-tidy -p ${CMAKE_BINARY_DIR} --quiet ${OMA_TIDY_SOURCES}
        COMMENT "clang-tidy"
        DEPENDS oma_compositor_shaders)
else()
    add_custom_target(tidy
        COMMAND clang-tidy -p ${CMAKE_BINARY_DIR} --quiet ${OMA_TIDY_SOURCES}
        COMMENT "clang-tidy")
endif()

add_custom_target(compdb
    COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_BINARY_DIR}/compile_commands.json ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "compile_commands.json -> build")
