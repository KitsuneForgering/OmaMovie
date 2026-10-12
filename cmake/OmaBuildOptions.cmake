# The four configurations of ADR-0001, now CMake build types. asan/tsan are not standard CMake
# types; their flags live in CMAKE_CXX_FLAGS_<TYPE>. Release keeps -g (packagers strip separately).
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_ASAN
    "-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all"
    CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_TSAN "-O1 -g -fno-omit-frame-pointer -fsanitize=thread" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_ASAN "-fsanitize=address,undefined -fno-sanitize-recover=all"
    CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_TSAN "-fsanitize=thread" CACHE STRING "" FORCE)

option(OMA_WERROR "Treat warnings as errors" ON)
option(OMA_BUILD_TESTS "Build the Cest test suites" ON)
option(OMA_BUILD_APP "Build the Qt Quick app" ON)
option(OMA_BUILD_SPIKES "Build the disposable M1 spikes" OFF)
option(OMA_BUILD_FUZZ "Build the libFuzzer targets (Clang only)" OFF)
set(OMA_LICENSE_NAME "omamovie" CACHE STRING "License directory name used by install()")
