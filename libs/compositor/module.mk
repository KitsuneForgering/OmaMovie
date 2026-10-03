# libs/compositor — render graph, CPU reference and Vulkan compositor. Depends on media, gpu, base.
# Shaders (libs/compositor/shaders/*) compile to SPIR-V at build time and are embedded as
# uint32 arrays (CLAUDE.md §9.5).
COMPOSITOR_GEN    := $(BUILD_DIR)/gen/compositor
COMPOSITOR_SHADERS := $(wildcard libs/compositor/shaders/*.comp)
COMPOSITOR_SPV    := $(patsubst libs/compositor/shaders/%,$(COMPOSITOR_GEN)/%.inc,$(COMPOSITOR_SHADERS))
GLSLC             ?= glslc

$(eval $(call oma_library,compositor,$(wildcard libs/compositor/src/*.cpp),media gpu base,-I$(COMPOSITOR_GEN),))

$(COMPOSITOR_GEN)/%.inc: libs/compositor/shaders/% $(MAKEFILE_LIST)
	$(call say,GLSLC,$<)
	@mkdir -p $(@D)
	$(Q)$(GLSLC) --target-env=vulkan1.3 -O -Werror -mfmt=num -o $@ $<

$(call obj_of,libs/compositor/src/vulkan_compositor.cpp): $(COMPOSITOR_SPV)
