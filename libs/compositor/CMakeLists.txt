# libs/compositor — render graph, CPU reference and Vulkan compositor. Depends on media, gpu, base.
# Shaders (shaders/*.comp) compile to SPIR-V at build time and are embedded as uint32 arrays (§9.5).
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

find_program(GLSLC glslc REQUIRED)
set(COMPOSITOR_GEN ${CMAKE_BINARY_DIR}/gen/compositor)
file(GLOB COMPOSITOR_SHADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp)

set(COMPOSITOR_SPV)
foreach(shader ${COMPOSITOR_SHADERS})
    get_filename_component(name ${shader} NAME)
    set(out ${COMPOSITOR_GEN}/${name}.inc)
    add_custom_command(
        OUTPUT ${out}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${COMPOSITOR_GEN}
        COMMAND ${GLSLC} --target-env=vulkan1.3 -O -Werror -mfmt=num -o ${out} ${shader}
        DEPENDS ${shader}
        COMMENT "GLSLC ${name}")
    list(APPEND COMPOSITOR_SPV ${out})
endforeach()

oma_library(compositor SOURCES ${SOURCES} ${COMPOSITOR_SPV} DEPS media gpu base)
target_include_directories(oma_compositor PRIVATE ${COMPOSITOR_GEN})
add_custom_target(oma_compositor_shaders DEPENDS ${COMPOSITOR_SPV})
