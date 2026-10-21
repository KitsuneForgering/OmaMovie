#pragma once

#include "oma/base/error.hpp"

#include <vulkan/vulkan_core.h>

#include <array>
#include <cstdint>
#include <span>
#include <string>

// Pieces of the NV12 pass shared by its source files (nv12_pass*.cpp).
namespace oma::compositor::detail {

[[nodiscard]] Error nv12_failure(VkResult r, std::string what);

// The push-descriptor set (a storage image, a storage buffer) and the pipeline layout with the
// row stride as a push constant. On failure the handles made so far are left for the caller.
[[nodiscard]] Result<void> create_nv12_layouts(VkDevice device, VkDescriptorSetLayout& set,
                                               VkPipelineLayout& layout);

// The two descriptor writes binding the source image and the buffer; `image` and `range` must
// outlive them.
[[nodiscard]] std::array<VkWriteDescriptorSet, 2> nv12_writes(const VkDescriptorImageInfo& image,
                                                              const VkDescriptorBufferInfo& range);

// A compute pipeline running `spv`'s main with `layout`.
[[nodiscard]] Result<VkPipeline> nv12_pipeline(VkDevice device, std::span<const std::uint32_t> spv,
                                               VkPipelineLayout layout);

// Makes the shader's buffer writes visible to the host (read after the runner's fence).
void nv12_host_barrier(VkCommandBuffer cmd);

} // namespace oma::compositor::detail
