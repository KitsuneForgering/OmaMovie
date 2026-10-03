# libs/gpu — OmaMovie's Vulkan device (Vulkan-Hpp confined to src/). Depends on base.
GPU_PKGS := vulkan
$(eval $(call oma_library,gpu,$(wildcard libs/gpu/src/*.cpp),base,$(shell pkg-config --cflags $(GPU_PKGS) 2>/dev/null),$(shell pkg-config --libs $(GPU_PKGS) 2>/dev/null)))
