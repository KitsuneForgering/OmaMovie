# The four configurations of ADR-0001, now CMake build types. asan/tsan are not standard CMake
# types; their flags live in CMAKE_CXX_FLAGS_<TYPE>. Release keeps -g (packagers strip separately).
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3" CACHE STRING "" FORCE)
set(OMA_RELEASE_OPT_LEVEL "2" CACHE STRING "Release compiler optimization level (2 or 3)")
set_property(CACHE OMA_RELEASE_OPT_LEVEL PROPERTY STRINGS 2 3)
if(NOT OMA_RELEASE_OPT_LEVEL MATCHES "^[23]$")
    message(FATAL_ERROR "OMA_RELEASE_OPT_LEVEL must be 2 or 3")
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O${OMA_RELEASE_OPT_LEVEL} -g -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_ASAN
    "-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all"
    CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_TSAN "-O1 -g -fno-omit-frame-pointer -fsanitize=thread" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_ASAN "-fsanitize=address,undefined -fno-sanitize-recover=all"
    CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_TSAN "-fsanitize=thread" CACHE STRING "" FORCE)

option(OMA_ENABLE_RELEASE_IPO "Enable compiler/linker IPO (LTO) for Release targets" OFF)
if(OMA_ENABLE_RELEASE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error LANGUAGES CXX)
    if(NOT ipo_supported)
        message(FATAL_ERROR "Release IPO is unavailable: ${ipo_error}")
    endif()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

option(OMA_WERROR "Treat warnings as errors" ON)
option(OMA_BUILD_TESTS "Build the Cest test suites" ON)
option(OMA_BUILD_APP "Build the Qt Quick app" ON)
option(OMA_BUILD_SPIKES "Build the disposable M1 spikes" OFF)
option(OMA_BUILD_FUZZ "Build the libFuzzer targets (Clang only)" OFF)
set(OMA_LICENSE_NAME "omamovie" CACHE STRING "License directory name used by install()")
