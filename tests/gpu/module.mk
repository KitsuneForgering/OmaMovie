# libs/gpu tests. They skip (and pass) on machines without a Vulkan driver; CI installs lavapipe.
$(eval $(call oma_test,gpu_tests,$(wildcard tests/gpu/*.cpp),gpu base))
