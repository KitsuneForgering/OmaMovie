#include "oma/gpu/device.hpp"
#include "oma/media/video_decoder.hpp"

#include "media_test.hpp"

#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "oma_test.hpp"

using oma::gpu::Device;
using oma::media::DecodePath;
using oma::media::GpuImages;
using oma::media::VideoDecoder;
using oma::media::VideoDecoderOptions;
using oma::media::VideoFrame;

namespace {

constexpr VkDeviceSize kReadbackBytes = 64;

// Consumes GPU frames on OmaMovie's graphics queue the way the compositor will: wait on the
// frame's semaphores, use the images, signal, commit. Optionally copies the first luma bytes
// back so the test can compare them with a software decode (verification only).
class Consumer {
public:
    explicit Consumer(const Device& d) : d_(d), queue_(d.queue(d.graphics_family(), 0)) {
        const VkDevice dev = d.device();
        const VkCommandPoolCreateInfo pci{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                          .pNext = nullptr,
                                          .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                          .queueFamilyIndex = d.graphics_family()};
        ok_ = vkCreateCommandPool(dev, &pci, nullptr, &pool_) == VK_SUCCESS;
        const VkCommandBufferAllocateInfo cai{.sType =
                                                  VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                              .pNext = nullptr,
                                              .commandPool = pool_,
                                              .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                              .commandBufferCount = 1};
        ok_ = ok_ && vkAllocateCommandBuffers(dev, &cai, &cmd_) == VK_SUCCESS;
        const VkFenceCreateInfo fci{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = 0};
        ok_ = ok_ && vkCreateFence(dev, &fci, nullptr, &fence_) == VK_SUCCESS;
        const VkBufferCreateInfo bci{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                     .pNext = nullptr,
                                     .flags = 0,
                                     .size = kReadbackBytes,
                                     .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                     .queueFamilyIndexCount = 0,
                                     .pQueueFamilyIndices = nullptr};
        ok_ = ok_ && vkCreateBuffer(dev, &bci, nullptr, &buffer_) == VK_SUCCESS;
        if (!ok_) {
            return;
        }
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(dev, buffer_, &req);
        VkPhysicalDeviceMemoryProperties mp{};
        vkGetPhysicalDeviceMemoryProperties(d.physical_device(), &mp);
        constexpr VkMemoryPropertyFlags kWant =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        uint32_t type = UINT32_MAX;
        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
            if ((req.memoryTypeBits & (1U << i)) != 0 &&
                (mp.memoryTypes[i].propertyFlags & kWant) == kWant) {
                type = i;
                break;
            }
        }
        const VkMemoryAllocateInfo mai{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                       .pNext = nullptr,
                                       .allocationSize = req.size,
                                       .memoryTypeIndex = type};
        ok_ = type != UINT32_MAX && vkAllocateMemory(dev, &mai, nullptr, &memory_) == VK_SUCCESS &&
              vkBindBufferMemory(dev, buffer_, memory_, 0) == VK_SUCCESS;
    }

    Consumer(const Consumer&) = delete;
    Consumer& operator=(const Consumer&) = delete;

    ~Consumer() {
        const VkDevice dev = d_.device();
        vkDestroyBuffer(dev, buffer_, nullptr);
        vkFreeMemory(dev, memory_, nullptr);
        vkDestroyFence(dev, fence_, nullptr);
        vkDestroyCommandPool(dev, pool_, nullptr);
    }

    [[nodiscard]] bool ok() const { return ok_; }

    // Returns the first luma bytes when `copy` is set; empty on failure.
    std::vector<uint8_t> consume(VideoFrame& frame, bool copy) {
        auto access = frame.acquire_gpu();
        if (!access) {
            return {};
        }
        const GpuImages& img = access->images();
        vkResetCommandBuffer(cmd_, 0);
        const VkCommandBufferBeginInfo begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                             .pNext = nullptr,
                                             .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                             .pInheritanceInfo = nullptr};
        vkBeginCommandBuffer(cmd_, &begin);
        std::vector<VkImageMemoryBarrier2> to_src;
        for (uint32_t i = 0; i < img.image_count; ++i) {
            to_src.push_back(
                barrier(img.images[i], img.layouts[i], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
        }
        dependency(to_src);
        if (copy) {
            const uint32_t bytes_per_texel = frame.bit_depth() > 8 ? 2 : 1;
            const VkImageAspectFlags aspect = img.image_count == 1 && frame.plane_count() > 1
                                                  ? VK_IMAGE_ASPECT_PLANE_0_BIT
                                                  : VK_IMAGE_ASPECT_COLOR_BIT;
            const VkBufferImageCopy region{
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {.aspectMask = aspect,
                                     .mipLevel = 0,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1},
                .imageOffset = {.x = 0, .y = 0, .z = 0},
                .imageExtent = {.width = static_cast<uint32_t>(kReadbackBytes) / bytes_per_texel,
                                .height = 1,
                                .depth = 1}};
            vkCmdCopyImageToBuffer(cmd_, img.images[0], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   buffer_, 1, &region);
        }
        std::vector<VkImageMemoryBarrier2> back;
        for (uint32_t i = 0; i < img.image_count; ++i) {
            back.push_back(barrier(img.images[i], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   VK_IMAGE_LAYOUT_GENERAL));
        }
        dependency(back);
        vkEndCommandBuffer(cmd_);

        std::vector<VkSemaphoreSubmitInfo> waits;
        std::vector<VkSemaphoreSubmitInfo> signals;
        for (uint32_t i = 0; i < img.image_count; ++i) {
            if (img.semaphores[i] == VK_NULL_HANDLE) {
                continue;
            }
            waits.push_back(semaphore(img.semaphores[i], img.wait_values[i]));
            signals.push_back(semaphore(img.semaphores[i], img.signal_values[i]));
        }
        const VkCommandBufferSubmitInfo cbi{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                                            .pNext = nullptr,
                                            .commandBuffer = cmd_,
                                            .deviceMask = 0};
        const VkSubmitInfo2 submit{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                                   .pNext = nullptr,
                                   .flags = 0,
                                   .waitSemaphoreInfoCount = static_cast<uint32_t>(waits.size()),
                                   .pWaitSemaphoreInfos = waits.data(),
                                   .commandBufferInfoCount = 1,
                                   .pCommandBufferInfos = &cbi,
                                   .signalSemaphoreInfoCount =
                                       static_cast<uint32_t>(signals.size()),
                                   .pSignalSemaphoreInfos = signals.data()};
        d_.lock_queue(d_.graphics_family(), 0);
        const VkResult r = vkQueueSubmit2(queue_, 1, &submit, fence_);
        d_.unlock_queue(d_.graphics_family(), 0);
        if (r != VK_SUCCESS) {
            return {};
        }
        access->commit(VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_2_MEMORY_READ_BIT);
        vkWaitForFences(d_.device(), 1, &fence_, VK_TRUE, UINT64_MAX);
        vkResetFences(d_.device(), 1, &fence_);
        ++consumed_;
        if (!copy) {
            return {};
        }
        void* p = nullptr;
        if (vkMapMemory(d_.device(), memory_, 0, kReadbackBytes, 0, &p) != VK_SUCCESS) {
            return {};
        }
        std::vector<uint8_t> out(static_cast<const uint8_t*>(p),
                                 static_cast<const uint8_t*>(p) + kReadbackBytes);
        vkUnmapMemory(d_.device(), memory_);
        return out;
    }

    [[nodiscard]] int consumed() const { return consumed_; }

private:
    static VkImageMemoryBarrier2 barrier(VkImage image, VkImageLayout from, VkImageLayout to) {
        return {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext = nullptr,
                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .oldLayout = from,
                .newLayout = to,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .baseMipLevel = 0,
                                     .levelCount = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1}};
    }

    static VkSemaphoreSubmitInfo semaphore(VkSemaphore s, uint64_t value) {
        return {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext = nullptr,
                .semaphore = s,
                .value = value,
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .deviceIndex = 0};
    }

    void dependency(const std::vector<VkImageMemoryBarrier2>& barriers) {
        const VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                   .pNext = nullptr,
                                   .dependencyFlags = 0,
                                   .memoryBarrierCount = 0,
                                   .pMemoryBarriers = nullptr,
                                   .bufferMemoryBarrierCount = 0,
                                   .pBufferMemoryBarriers = nullptr,
                                   .imageMemoryBarrierCount =
                                       static_cast<uint32_t>(barriers.size()),
                                   .pImageMemoryBarriers = barriers.data()};
        vkCmdPipelineBarrier2(cmd_, &dep);
    }

    const Device& d_;
    VkQueue queue_ = VK_NULL_HANDLE;
    VkCommandPool pool_ = VK_NULL_HANDLE;
    VkCommandBuffer cmd_ = VK_NULL_HANDLE;
    VkFence fence_ = VK_NULL_HANDLE;
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    bool ok_ = false;
    int consumed_ = 0;
};

// First luma bytes of the first frame decoded in software, for comparison.
std::vector<uint8_t> software_luma(const char* name) {
    VideoDecoderOptions o;
    o.paths = {DecodePath::Software};
    auto d = VideoDecoder::open(fixture(name), o);
    if (!d) {
        return {};
    }
    auto f = (*d)->next();
    if (!f || !*f) {
        return {};
    }
    auto plane = (*f)->plane(0);
    return {plane.begin(), plane.begin() + static_cast<std::ptrdiff_t>(std::min<std::size_t>(
                                               kReadbackBytes, plane.size()))};
}

// Hardware formats with more than 8 bits (P010) keep samples in the high bits; FFmpeg's
// software layouts (yuv420p10le) keep them in the low bits. Compares samples, not bytes.
bool same_luma(const std::vector<uint8_t>& gpu, const std::vector<uint8_t>& sw, int bit_depth) {
    if (gpu.size() != sw.size() || gpu.empty()) {
        return false;
    }
    if (bit_depth <= 8) {
        return gpu == sw;
    }
    const int shift = 16 - bit_depth;
    for (std::size_t i = 0; i + 1 < gpu.size(); i += 2) {
        const int g = (gpu[i] | (gpu[i + 1] << 8)) >> shift;
        const int s = sw[i] | (sw[i + 1] << 8);
        if (g != s) {
            return false;
        }
    }
    return true;
}

void decode_on_gpu(const char* name) {
    const Device* device = media_test_device();
    if (device == nullptr || !have_fixture(name)) {
        return;
    }
    VideoDecoderOptions o;
    o.device = device;
    auto d = VideoDecoder::open(fixture(name), o);
    expect(d.has_value()).toBeTruthy();
    if (!d) {
        return;
    }
    Consumer consumer(*device);
    expect(consumer.ok()).toBeTruthy();
    if (!consumer.ok()) {
        return;
    }
    int frames = 0;
    int on_gpu = 0;
    std::vector<uint8_t> gpu_luma;
    int bit_depth = 8;
    for (;;) {
        auto f = (*d)->next();
        expect(f.has_value()).toBeTruthy();
        if (!f || !*f) {
            break;
        }
        ++frames;
        VideoFrame& frame = **f;
        expect(frame.path() == (*d)->path()).toBeTruthy();
        if (!frame.on_gpu()) {
            continue;
        }
        ++on_gpu;
        const bool first = gpu_luma.empty();
        auto luma = consumer.consume(frame, first);
        if (first) {
            gpu_luma = std::move(luma);
            bit_depth = frame.bit_depth();
            // Hardware layouts interleave chroma (NV12/P010); P010 keeps samples in the high bits.
            expect(frame.layout().interleaved_chroma).toBeTruthy();
            expect(frame.layout().lsb_shift).toEqual(bit_depth > 8 ? 16 - bit_depth : 0);
        }
    }
    std::printf("    %s: %s, %d frames, %d on the GPU\n", name,
                std::string(oma::media::to_string((*d)->path())).c_str(), frames, on_gpu);
    expect(frames).toEqual(30);
    if ((*d)->path() == DecodePath::Software) {
        expect(on_gpu).toEqual(0);
        return;
    }
    expect(on_gpu).toEqual(frames);
    expect(consumer.consumed()).toEqual(frames);
    expect(same_luma(gpu_luma, software_luma(name), bit_depth)).toBeTruthy();
}

// Hardware usually refuses 4:4:4 H.264: the decoder must reopen in software by itself and still
// deliver every frame from the start.
void falls_back_to_software() {
    const Device* device = media_test_device();
    if (device == nullptr || !have_fixture("h264_444.mp4")) {
        return;
    }
    VideoDecoderOptions o;
    o.device = device;
    auto d = VideoDecoder::open(fixture("h264_444.mp4"), o);
    expect(d.has_value()).toBeTruthy();
    if (!d) {
        return;
    }
    int frames = 0;
    std::int64_t first_pts = -1;
    for (;;) {
        auto f = (*d)->next();
        expect(f.has_value()).toBeTruthy();
        if (!f || !*f) {
            break;
        }
        if (frames == 0) {
            first_pts = (*f)->pts().value_or(oma::RationalTime()).value();
        }
        ++frames;
    }
    std::printf("    h264_444.mp4: %s\n", std::string(oma::media::to_string((*d)->path())).c_str());
    expect(frames).toEqual(30);
    expect(first_pts).toEqual(0);
}

// Without Software in the list, a refusal is an error rather than a silent downgrade.
void refusal_without_fallback() {
    const Device* device = media_test_device();
    if (device == nullptr || !have_fixture("h264_444.mp4")) {
        return;
    }
    VideoDecoderOptions o;
    o.device = device;
    o.paths = {DecodePath::VaapiToVulkan, DecodePath::VulkanVideo};
    auto d = VideoDecoder::open(fixture("h264_444.mp4"), o);
    if (!d) {
        expect(code_of(d.error())).toEqual(static_cast<int>(oma::ErrorCode::Unsupported));
        return;
    }
    auto f = (*d)->next();
    // Either the hardware takes 4:4:4 after all, or the refusal surfaces as Unsupported.
    expect((f && *f && (*f)->on_gpu()) || (!f && f.error().code() == oma::ErrorCode::Unsupported))
        .toBeTruthy();
}

} // namespace

void run_gpu_decode_tests() {
    describe("media::VideoDecoder (GPU paths)", {
        it("decodes H.264 onto OmaMovie's device", { decode_on_gpu("h264_30fps_aac.mp4"); });
        it("decodes 10-bit HEVC onto OmaMovie's device", { decode_on_gpu("hevc_10bit.mp4"); });
        it("decodes AV1 onto OmaMovie's device", { decode_on_gpu("av1_opus.mkv"); });
        it("falls back to software when the driver refuses a stream",
           { falls_back_to_software(); });
        it("reports a refusal when software is not allowed", { refusal_without_fallback(); });
    });
}
