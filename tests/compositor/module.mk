# libs/compositor tests: geometry and color math, the CPU reference, and GPU-vs-CPU comparisons
# (GPU cases skip without a Vulkan driver; CI runs them on lavapipe).
$(eval $(call oma_test,compositor_tests,$(wildcard tests/compositor/*.cpp),compositor media gpu base))
