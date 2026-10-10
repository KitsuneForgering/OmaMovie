# Disposable M1 spikes (Docs/spikes/). Opt-in: cmake --preset spikes. They need the Vulkan,
# FFmpeg, libplacebo and Qt development files; the shell scripts (s1, s3) are not built.
pkg_check_modules(SPIKE_AV REQUIRED libavformat libavcodec libavutil)

# s2: FFmpeg decoding into an AVVkFrame on OmaMovie's device (Vulkan + FFmpeg only).
add_executable(s2_ffmpeg_own_device s2_ffmpeg_own_device.cpp)
target_compile_options(s2_ffmpeg_own_device PRIVATE -Wall -Wextra)
target_include_directories(s2_ffmpeg_own_device SYSTEM PRIVATE ${SPIKE_AV_INCLUDE_DIRS})
target_link_libraries(s2_ffmpeg_own_device PRIVATE ${SPIKE_AV_LIBRARIES} ${VULKAN_LIBRARIES})

# s5: drives the library decoder and compositor directly.
add_executable(s5_minimal_compositing s5_minimal_compositing.cpp)
target_compile_options(s5_minimal_compositing PRIVATE ${OMA_TEST_WARNINGS})
target_link_libraries(s5_minimal_compositing PRIVATE
    oma_compositor oma_media oma_gpu oma_base)

# s6: imports OmaMovie's device into libplacebo.
pkg_check_modules(LIBPLACEBO REQUIRED libplacebo)
add_executable(s6_libplacebo s6_libplacebo.cpp)
target_compile_options(s6_libplacebo PRIVATE -Wall -Wextra)
target_include_directories(s6_libplacebo SYSTEM PRIVATE ${LIBPLACEBO_INCLUDE_DIRS})
target_link_libraries(s6_libplacebo PRIVATE
    oma_compositor oma_media oma_gpu oma_base ${LIBPLACEBO_LIBRARIES})

# s4: reuses the real compositor and Qt's RHI (find_package gives the versioned headers).
find_package(Qt6 REQUIRED COMPONENTS Quick)
add_executable(s4_qt_shared_device s4_qt_shared_device.cpp)
target_compile_options(s4_qt_shared_device PRIVATE ${OMA_TEST_WARNINGS})
target_link_libraries(s4_qt_shared_device PRIVATE
    oma_compositor oma_media oma_gpu oma_base Qt6::Quick)
