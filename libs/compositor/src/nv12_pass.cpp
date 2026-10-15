#include "nv12_pass.hpp"

#include "nv12_pass_detail.hpp"

#include "oma/gpu/device.hpp"

namespace oma::compositor::detail {

namespace {
constexpr std::uint32_t kBlocksX = 16; // nv12_bt709.comp's local size, in 4x2 pixel blocks
constexpr std::uint32_t kBlocksY = 8;
} // namespace

Nv12Pass::~Nv12Pass() {
    VkDevice dev = device_->device();
    vkDestroyPipeline(dev, pipeline_, nullptr);
    vkDestroyPipelineLayout(dev, layout_, nullptr);
    vkDestroyDescriptorSetLayout(dev, set_layout_, nullptr);
}

Result<std::span<const std::uint8_t>> Nv12Pass::run(gpu::CommandRunner& runner, VkImageView source,
                                                    std::uint32_t width, std::uint32_t height,
                                                    std::uint32_t stride) {
    const VkDeviceSize bytes = static_cast<VkDeviceSize>(stride) * height * 3 / 2;
    if (buffer_.size() < bytes) {
        auto made = gpu::Buffer::create(*device_, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                        gpu::MemoryUse::Readback);
        if (!made)
            return std::unexpected(made.error());
        buffer_ = std::move(*made);
    }
    const auto ran = runner.run([&](VkCommandBuffer cmd) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
        const VkDescriptorImageInfo image{
            .sampler = VK_NULL_HANDLE, .imageView = source, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
        const VkDescriptorBufferInfo range{.buffer = buffer_.handle(), .offset = 0, .range = bytes};
        const auto writes = nv12_writes(image, range);
        push_descriptors_(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout_, 0, 2, writes.data());
        vkCmdPushConstants(cmd, layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(stride), &stride);
        const std::uint32_t blocks_x = (width + 3) / 4;
        vkCmdDispatch(cmd, (blocks_x + kBlocksX - 1) / kBlocksX,
                      ((height / 2) + kBlocksY - 1) / kBlocksY, 1);
        nv12_host_barrier(cmd);
    });
    if (!ran)
        return std::unexpected(ran.error());
    buffer_.invalidate();
    return std::span(static_cast<const std::uint8_t*>(buffer_.mapped()), bytes);
}

} // namespace oma::compositor::detail
