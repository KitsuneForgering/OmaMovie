#pragma once

// Device memory around a compositor render (test_effect_memory.cpp).
#include "oma/gpu/device.hpp"

#include "compositor_test.hpp"

// This process's device memory in use, summed over heaps (VK_EXT_memory_budget's heap usage).
VkDeviceSize heap_usage(const oma::gpu::Device& device);

// MiB a fresh compositor gains when `layer`'s effect is added to a warmed render of the source.
double added_mib(const oma::gpu::Device& device, DecodedFrame& source, std::uint32_t w,
                 std::uint32_t h, const oma::compositor::Layer& layer);
