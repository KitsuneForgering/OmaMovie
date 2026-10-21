// write_nv12 (M12, GPU-native encoder input) against read_nv12: the planes the GPU writes must
// hold exactly the bytes the readback returns, row for row.
#include "oma/compositor/compositor.hpp"
#include "oma/gpu/commands.hpp"
#include "oma/gpu/device.hpp"
#include "oma/gpu/resources.hpp"

#include "compositor_test.hpp"

#include <array>
#include <cstdio>
#include <cstring>

#include "oma_test.hpp"

namespace oc = oma::compositor;
namespace gpu = oma::gpu;

namespace {

// The bytes of `image` (`texel` bytes each), copied out row by row.
std::vector<std::uint8_t> bytes_of(const gpu::Device& device, const gpu::Image& image,
                                   std::uint32_t texel) {
    const std::uint32_t w = image.desc().width;
    const std::uint32_t h = image.desc().height;
    auto buffer = gpu::Buffer::create(device, VkDeviceSize{w} * h * texel,
                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT, gpu::MemoryUse::Readback);
    auto runner = gpu::CommandRunner::create(device, device.graphics_family());
    if (!buffer || !runner)
        return {};
    const bool ran = runner
                         ->run([&](VkCommandBuffer cmd) {
                             const VkBufferImageCopy region{
                                 .bufferOffset = 0,
                                 .bufferRowLength = 0,
                                 .bufferImageHeight = 0,
                                 .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                 .imageOffset = {0, 0, 0},
                                 .imageExtent = {w, h, 1}};
                             vkCmdCopyImageToBuffer(cmd, image.handle(), VK_IMAGE_LAYOUT_GENERAL,
                                                    buffer->handle(), 1, &region);
                         })
                         .has_value();
    if (!ran)
        return {};
    buffer->invalidate();
    const auto* data = static_cast<const std::uint8_t*>(buffer->mapped());
    return {data, data + (std::size_t{w} * h * texel)};
}

// A camera frame composited at 318x180 (a width that is no multiple of 4, as read_nv12 pads).
void planes_equal_readback() {
    const gpu::Device* device = compositor_test_device();
    auto src = decode_first("h264_30fps_aac.mp4");
    if (device == nullptr || !src.frame)
        return;
    auto vk = oc::VulkanCompositor::create(*device);
    oc::RenderGraph g;
    g.width = 318;
    g.height = 180;
    g.layers.resize(1);
    const std::array<oc::LayerInput, 1> in{src.input()};
    constexpr VkImageUsageFlags kUsage =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    auto luma = gpu::Image::create(
        *device, {.width = 318, .height = 180, .format = VK_FORMAT_R8_UNORM, .usage = kUsage});
    auto cbcr = gpu::Image::create(
        *device, {.width = 159, .height = 90, .format = VK_FORMAT_R8G8_UNORM, .usage = kUsage});
    expect(vk && luma && cbcr && (*vk)->render(g, in).has_value()).toBeTruthy();
    if (!vk || !luma || !cbcr)
        return;
    const auto nv12 = (*vk)->read_nv12();
    const bool written = (*vk)
                             ->write_nv12({.luma = luma->handle(),
                                           .luma_view = luma->view(),
                                           .cbcr = cbcr->handle(),
                                           .cbcr_view = cbcr->view(),
                                           .foreign = false})
                             .has_value();
    const auto y = bytes_of(*device, *luma, 1);
    const auto uv = bytes_of(*device, *cbcr, 2);
    expect(nv12 && written && y.size() == 318U * 180 && uv.size() == 318U * 90).toBeTruthy();
    if (!nv12 || y.empty() || uv.empty())
        return;
    int differing = 0;
    for (std::size_t row = 0; row < 180; ++row)
        differing += std::memcmp(&y[row * 318], &nv12->bytes[row * nv12->stride], 318) != 0 ? 1 : 0;
    for (std::size_t row = 0; row < 90; ++row)
        differing +=
            std::memcmp(&uv[row * 318], &nv12->bytes[(180 + row) * nv12->stride], 318) != 0 ? 1 : 0;
    std::printf("    planes vs readback: %d of 270 rows differ\n", differing);
    expect(differing).toBe(0);
    g.width = 319; // odd: refused, as read_nv12 refuses it
    expect((*vk)->render(g, in).has_value() && !(*vk)->write_nv12({}).has_value()).toBeTruthy();
}

} // namespace

void run_nv12_planes_tests() {
    describe("compositor::VulkanCompositor::write_nv12", {
        it("writes the same bytes into two planes as read_nv12 returns",
           { planes_equal_readback(); });
    });
}
