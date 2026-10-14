# --- libraries (CLAUDE.md §5.1) ---
add_subdirectory(libs/base)
add_subdirectory(libs/gpu)
add_subdirectory(libs/media)
add_subdirectory(libs/compositor)
add_subdirectory(libs/audio)
add_subdirectory(libs/timeline)
add_subdirectory(libs/playback)
add_subdirectory(libs/project-ir)
add_subdirectory(libs/importers)
add_subdirectory(libs/project)

# A fuzz build instruments libraries for libFuzzer (tests/fuzz), which only fuzzer executables link.
if(NOT OMA_BUILD_FUZZ)
    add_subdirectory(tools/oma-project)
endif()

# --- tests ---
# Fixtures: the ffmpeg generator, stamped so it runs once (like the old TEST_PREREQS).
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/tests/fixtures/generated/.stamp
    COMMAND ${CMAKE_SOURCE_DIR}/tests/fixtures/generate.sh
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_SOURCE_DIR}/tests/fixtures/generated/.stamp
    DEPENDS ${CMAKE_SOURCE_DIR}/tests/fixtures/generate.sh
    COMMENT "Generating test fixtures (ffmpeg)")
add_custom_target(fixtures DEPENDS ${CMAKE_SOURCE_DIR}/tests/fixtures/generated/.stamp)

if(OMA_BUILD_TESTS)
    add_subdirectory(tests/base)
    add_subdirectory(tests/gpu)
    add_subdirectory(tests/media)
    add_subdirectory(tests/compositor)
    add_subdirectory(tests/audio)
    add_subdirectory(tests/timeline)
    add_subdirectory(tests/playback)
    add_subdirectory(tests/project-ir)
    add_subdirectory(tests/importers)
    add_subdirectory(tests/project)
    add_subdirectory(tests/oma-project)
endif()

# The app and its headless Session tests, in every build: the sanitizer builds run them too (M10
# application sanitizer subset). Qt, GLib and libdbus are not instrumented; what that leaves out
# and why is recorded beside each entry in tests/support/{lsan,tsan}.supp.
if(OMA_BUILD_APP)
    add_subdirectory(apps/omamovie)
endif()

if(OMA_BUILD_SPIKES)
    add_subdirectory(tools/spikes)
endif()
if(OMA_BUILD_FUZZ)
    add_subdirectory(tests/fuzz)
endif()
