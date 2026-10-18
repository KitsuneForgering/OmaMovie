# libs/gpu — OmaMovie's Vulkan device (Vulkan-Hpp confined to src/). Depends on base.
# The Vulkan headers are PUBLIC: media's public frame types use VkFormat/VkImage (CLAUDE.md §5.2).
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
pkg_check_modules(VULKAN REQUIRED vulkan)
oma_library(gpu SOURCES ${SOURCES} DEPS base)
target_include_directories(oma_gpu SYSTEM PUBLIC ${VULKAN_INCLUDE_DIRS})
target_link_libraries(oma_gpu PRIVATE ${VULKAN_LIBRARIES})
