#include "vulkan_device.hpp"

#include "oma/base/log.hpp"
#include "oma/gpu/device.hpp"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>
}

#include <algorithm>
#include <cstddef>

namespace oma::media::ff {

namespace {

#if FF_API_VULKAN_SYNC_QUEUES
const gpu::Device& device_of(const AVHWDeviceContext* ctx) {
    return *static_cast<const gpu::Device*>(ctx->user_opaque);
}
// The signatures are FFmpeg's callback types, so the context cannot be const.
// NOLINTNEXTLINE(misc-const-correctness)
void lock_queue(AVHWDeviceContext* ctx, uint32_t family, uint32_t index) {
    device_of(ctx).lock_queue(family, index);
}
// NOLINTNEXTLINE(misc-const-correctness)
void unlock_queue(AVHWDeviceContext* ctx, uint32_t family, uint32_t index) {
    device_of(ctx).unlock_queue(family, index);
}
#endif

} // namespace

Result<BufferPtr> wrap_vulkan_device(const gpu::Device& device) {
    ensure_initialized();
    BufferPtr ref(av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VULKAN));
    if (!ref) {
        return make_error(ErrorCode::Internal, Category::Gpu, "cannot allocate an FFmpeg device");
    }
    auto* hw = reinterpret_cast<AVHWDeviceContext*>(ref->data);
    auto* vk = static_cast<AVVulkanDeviceContext*>(hw->hwctx);
    hw->user_opaque = const_cast<gpu::Device*>(&device);

    vk->get_proc_addr = device.instance_proc_addr();
    vk->inst = device.instance();
    vk->phys_dev = device.physical_device();
    vk->act_dev = device.device();
    // The pNext chain points into the Device, which outlives this context.
    vk->device_features = device.enabled_features();
    vk->enabled_dev_extensions = device.enabled_extensions().data();
    vk->nb_enabled_dev_extensions = static_cast<int>(device.enabled_extensions().size());

    const auto families = device.queue_families();
    const std::size_t count = std::min(families.size(), std::size(vk->qf));
    for (std::size_t i = 0; i < count; ++i) {
        vk->qf[i] = {.idx = static_cast<int>(families[i].index),
                     .num = static_cast<int>(families[i].count),
                     .flags = static_cast<VkQueueFlagBits>(families[i].flags),
                     .video_caps =
                         static_cast<VkVideoCodecOperationFlagBitsKHR>(families[i].video_codecs)};
    }
    vk->nb_qf = static_cast<int>(count);
    vk->queue_flags = device.queue_create_flags();

    if (!device.internally_synchronized_queues()) {
#if FF_API_VULKAN_SYNC_QUEUES
        // Without internally synchronized queues FFmpeg must take the same per-queue locks as
        // the compositor and Qt. The hooks are deprecated only in favor of that extension.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        vk->lock_queue = lock_queue;
        vk->unlock_queue = unlock_queue;
#pragma GCC diagnostic pop
#else
        log_warn(Category::Gpu, "queues are not internally synchronized and FFmpeg no longer "
                                "accepts queue locks: concurrent submissions are unsafe");
#endif
    }

    if (const int err = av_hwdevice_ctx_init(ref.get()); err < 0) {
        return std::unexpected(
            av_error(err, Category::Gpu, "FFmpeg rejected OmaMovie's Vulkan device"));
    }
    return ref;
}

} // namespace oma::media::ff
