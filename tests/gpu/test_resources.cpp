#include "oma/gpu/commands.hpp"
#include "oma/gpu/device.hpp"
#include "oma/gpu/resources.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

#include "oma_test.hpp"

using oma::gpu::Buffer;
using oma::gpu::CommandRunner;
using oma::gpu::Device;
using oma::gpu::Image;
using oma::gpu::ImageDesc;
using oma::gpu::MemoryUse;

oma::gpu::Device* gpu_test_device();

namespace {

constexpr VkDeviceSize kBytes = 4096;

// Upload -> device-local -> readback through the GPU, then compare.
void round_trip_through_the_gpu() {
    Device* d = gpu_test_device();
    if (d == nullptr) {
        return;
    }
    auto upload = Buffer::create(*d, kBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryUse::Upload);
    auto local = Buffer::create(*d, kBytes,
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                MemoryUse::GpuOnly);
    auto readback =
        Buffer::create(*d, kBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryUse::Readback);
    auto runner = CommandRunner::create(*d, d->graphics_family());
    expect(upload && local && readback && runner).toBeTruthy();
    if (!upload || !local || !readback || !runner) {
        return;
    }
    expect(upload->mapped() != nullptr).toBeTruthy();
    expect(local->mapped() == nullptr).toBeTruthy();
    std::vector<std::uint8_t> pattern(kBytes);
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        pattern[i] = static_cast<std::uint8_t>((i * 31U) + 7U);
    }
    std::memcpy(upload->mapped(), pattern.data(), pattern.size());
    upload->flush();
    const VkBuffer src = upload->handle();
    const VkBuffer mid = local->handle();
    const VkBuffer dst = readback->handle();
    auto ran = runner->run([&](VkCommandBuffer cmd) {
        const VkBufferCopy region{.srcOffset = 0, .dstOffset = 0, .size = kBytes};
        vkCmdCopyBuffer(cmd, src, mid, 1, &region);
        const VkMemoryBarrier2 barrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                       .pNext = nullptr,
                                       .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                                       .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                       .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                                       .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT};
        const VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                   .pNext = nullptr,
                                   .dependencyFlags = 0,
                                   .memoryBarrierCount = 1,
                                   .pMemoryBarriers = &barrier,
                                   .bufferMemoryBarrierCount = 0,
                                   .pBufferMemoryBarriers = nullptr,
                                   .imageMemoryBarrierCount = 0,
                                   .pImageMemoryBarriers = nullptr};
        vkCmdPipelineBarrier2(cmd, &dep);
        vkCmdCopyBuffer(cmd, mid, dst, 1, &region);
    });
    expect(ran.has_value()).toBeTruthy();
    readback->invalidate();
    expect(std::memcmp(readback->mapped(), pattern.data(), pattern.size()) == 0).toBeTruthy();
}

void creates_images() {
    Device* d = gpu_test_device();
    if (d == nullptr) {
        return;
    }
    const ImageDesc desc{.width = 64,
                         .height = 32,
                         .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                         .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT};
    auto img = Image::create(*d, desc);
    expect(img.has_value()).toBeTruthy();
    if (!img) {
        return;
    }
    expect(img->handle() != VK_NULL_HANDLE).toBeTruthy();
    expect(img->view() != VK_NULL_HANDLE).toBeTruthy();
    expect(img->desc().width).toEqual(64);
    Image moved = std::move(*img);
    expect(moved.handle() != VK_NULL_HANDLE).toBeTruthy();
    expect(img->handle() == VK_NULL_HANDLE).toBeTruthy();
}

} // namespace

void run_resource_tests() {
    describe("gpu::Buffer and gpu::CommandRunner", {
        it("round-trips bytes through device-local memory", { round_trip_through_the_gpu(); });
        it("creates images with views and moves them", { creates_images(); });
    });
}
