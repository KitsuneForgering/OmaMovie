// Spike S2: can FFmpeg 9 decode onto a VkDevice that OmaMovie creates with Vulkan-Hpp,
// delivering AVVkFrame images without a CPU readback?
//
// Usage: s2_ffmpeg_own_device <media> [--mode vaapi|vulkan] [--frames N]
//
//   vaapi   (default) VA-API decode, frames mapped VA-API -> DRM (DMA-BUF) -> Vulkan on our device
//   vulkan  Vulkan Video decode directly on our device (needs video queues; on Intel TigerLake
//           only with ANV_DEBUG=video-decode,video-encode)
//
// For each frame the program checks that the result is an AVVkFrame on our device, follows the
// AVVkFrame synchronization contract on our own queue (wait sem_value, signal sem_value + 1), and
// for the first frame copies a few luma samples back to compare with a software decode. That
// single copy is verification only; the hot path never reads frames back.
//
// Disposable spike code (Docs/spikes/S2-*.md). What proves useful moves to libs/gpu and libs/media.

// Vulkan-Hpp without exceptions, returning std::expected (needs <expected> first and
// VULKAN_HPP_USE_STD_EXPECTED; otherwise it falls back to its own ResultValue type).
#include <expected>
#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_RAII_NO_EXCEPTIONS
#define VULKAN_HPP_USE_STD_EXPECTED
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_drm.h>
#include <libavutil/hwcontext_vulkan.h>
#include <libavutil/pixdesc.h>
}

#include <sys/resource.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

[[noreturn]] void die(const std::string& msg) {
    std::fprintf(stderr, "s2: %s\n", msg.c_str());
    std::exit(1);
}

std::string av_err(int err) {
    std::array<char, AV_ERROR_MAX_STRING_SIZE> buf{};
    av_strerror(err, buf.data(), buf.size());
    return buf.data();
}

template <typename T>
T expect_vk(std::expected<T, vk::Result> r, const char* what) {
    if (!r) {
        die(std::string(what) + ": " + vk::to_string(r.error()));
    }
    return std::move(*r);
}

void check_vk(std::expected<void, vk::Result> r, const char* what) {
    if (!r) {
        die(std::string(what) + ": " + vk::to_string(r.error()));
    }
}

double cpu_seconds() {
    rusage ru{};
    getrusage(RUSAGE_SELF, &ru);
    return static_cast<double>(ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) +
           static_cast<double>(ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1e6;
}

// ------------------------------------------------------------------ OmaMovie's Vulkan device

struct OwnDevice {
    vk::raii::Context context;
    vk::raii::Instance instance{nullptr};
    vk::raii::PhysicalDevice physical{nullptr};
    vk::raii::Device device{nullptr};

    std::vector<const char*> device_extensions;
    std::vector<AVVulkanDeviceQueueFamily> families;
    uint32_t graphics_family = 0;
    bool internally_synchronized = false;

    // Feature chain enabled at device creation; FFmpeg reads it through device_features.
    vk::PhysicalDeviceFeatures2 features2{};
    vk::PhysicalDeviceVulkan11Features v11{};
    vk::PhysicalDeviceVulkan12Features v12{};
    vk::PhysicalDeviceVulkan13Features v13{};
    vk::PhysicalDeviceVulkan14Features v14{};
    vk::PhysicalDeviceInternallySynchronizedQueuesFeaturesKHR isq{};
    vk::PhysicalDeviceVideoMaintenance1FeaturesKHR video_maint1{};
    vk::PhysicalDeviceDescriptorBufferFeaturesEXT descriptor_buffer{};
};

void create_own_device(OwnDevice& d) {
    const vk::ApplicationInfo app{.pApplicationName = "oma-spike-s2",
                                  .applicationVersion = 1,
                                  .pEngineName = "OmaMovie",
                                  .engineVersion = 1,
                                  .apiVersion = VK_API_VERSION_1_4};
    const vk::InstanceCreateInfo ici{.pApplicationInfo = &app};
    d.instance = expect_vk(d.context.createInstance(ici), "createInstance");

    auto physicals = expect_vk(d.instance.enumeratePhysicalDevices(), "enumeratePhysicalDevices");
    if (physicals.empty()) {
        die("no Vulkan device");
    }
    d.physical = std::move(physicals.front());
    const auto props = d.physical.getProperties();
    std::printf("device: %s (api %u.%u.%u, driver %u)\n", props.deviceName.data(),
                VK_API_VERSION_MAJOR(props.apiVersion), VK_API_VERSION_MINOR(props.apiVersion),
                VK_API_VERSION_PATCH(props.apiVersion), props.driverVersion);

    // Extensions: what FFmpeg can use (and the device has) plus internally synchronized queues.
    std::set<std::string> available;
    for (const auto& e : expect_vk(d.physical.enumerateDeviceExtensionProperties(),
                                   "enumerateDeviceExtensionProperties")) {
        available.insert(e.extensionName.data());
    }
    const std::set<std::string> with_feature_structs = {
        VK_KHR_VIDEO_MAINTENANCE_1_EXTENSION_NAME, VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
        VK_KHR_INTERNALLY_SYNCHRONIZED_QUEUES_EXTENSION_NAME};
    int n_opt = 0;
    const char* const* opt = av_vk_get_optional_device_extensions(&n_opt);
    std::set<std::string> wanted(opt, opt + n_opt);
    wanted.insert(VK_KHR_INTERNALLY_SYNCHRONIZED_QUEUES_EXTENSION_NAME);
    av_free(const_cast<char**>(opt));
    for (const auto& name : wanted) {
        if (!available.contains(name)) {
            continue;
        }
        // Only enable extensions whose features we chain below, or that have no features,
        // so the enabled feature set stays consistent with the enabled extensions.
        const bool known_featureless = name.find("external_") != std::string::npos ||
                                       name.find("drm_format_modifier") != std::string::npos ||
                                       name.find("video_queue") != std::string::npos ||
                                       name.find("video_decode") != std::string::npos ||
                                       name.find("video_encode") != std::string::npos ||
                                       name == VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME;
        if (known_featureless || with_feature_structs.contains(name)) {
            d.device_extensions.push_back(strdup(name.c_str()));
        }
    }
    auto enabled = [&](const char* name) {
        return std::ranges::any_of(d.device_extensions,
                                   [&](const char* e) { return std::strcmp(e, name) == 0; });
    };

    // Feature chain: query what is supported and enable exactly that.
    void** tail = &d.features2.pNext;
    auto link = [&](auto& s) {
        *tail = &s;
        tail = &s.pNext;
    };
    link(d.v11);
    link(d.v12);
    link(d.v13);
    link(d.v14);
    if (enabled(VK_KHR_INTERNALLY_SYNCHRONIZED_QUEUES_EXTENSION_NAME)) {
        link(d.isq);
    }
    if (enabled(VK_KHR_VIDEO_MAINTENANCE_1_EXTENSION_NAME)) {
        link(d.video_maint1);
    }
    if (enabled(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME)) {
        link(d.descriptor_buffer);
    }
    vkGetPhysicalDeviceFeatures2(static_cast<VkPhysicalDevice>(*d.physical),
                                 reinterpret_cast<VkPhysicalDeviceFeatures2*>(&d.features2));
    d.internally_synchronized = d.isq.internallySynchronizedQueues == vk::True;

    // Queues: every family, every queue, flagged internally synchronized when supported.
    // Video queue properties may only be chained when the device supports video queues.
    using QueueChain = vk::StructureChain<vk::QueueFamilyProperties2, vk::QueueFamilyVideoPropertiesKHR>;
    std::vector<QueueChain> qprops;
    if (available.contains(VK_KHR_VIDEO_QUEUE_EXTENSION_NAME)) {
        qprops = d.physical.getQueueFamilyProperties2<QueueChain>();
    } else {
        for (const auto& p : d.physical.getQueueFamilyProperties2()) {
            // Copy only the payload: assigning the whole struct would overwrite the chain's pNext.
            QueueChain c;
            c.get<vk::QueueFamilyProperties2>().queueFamilyProperties = p.queueFamilyProperties;
            qprops.push_back(c);
        }
    }
    std::vector<vk::DeviceQueueCreateInfo> qcis;
    std::vector<std::vector<float>> priorities(qprops.size());
    const vk::DeviceQueueCreateFlags qflags =
        d.internally_synchronized ? vk::DeviceQueueCreateFlagBits::eInternallySynchronizedKHR
                                  : vk::DeviceQueueCreateFlags{};
    bool found_graphics = false;
    for (uint32_t i = 0; i < qprops.size(); ++i) {
        const auto& qp = qprops[i].get<vk::QueueFamilyProperties2>().queueFamilyProperties;
        const auto& video = qprops[i].get<vk::QueueFamilyVideoPropertiesKHR>();
        priorities[i].assign(qp.queueCount, 1.0F);
        qcis.push_back({.flags = qflags,
                        .queueFamilyIndex = i,
                        .queueCount = qp.queueCount,
                        .pQueuePriorities = priorities[i].data()});
        d.families.push_back({.idx = static_cast<int>(i),
                              .num = static_cast<int>(qp.queueCount),
                              .flags = static_cast<VkQueueFlagBits>(
                                  static_cast<VkQueueFlags>(qp.queueFlags)),
                              .video_caps = static_cast<VkVideoCodecOperationFlagBitsKHR>(
                                  static_cast<VkVideoCodecOperationFlagsKHR>(
                                      video.videoCodecOperations))});
        if (!found_graphics && (qp.queueFlags & vk::QueueFlagBits::eGraphics)) {
            d.graphics_family = i;
            found_graphics = true;
        }
        std::printf("queue family %u: %u queue(s), flags %s, video %s\n", i, qp.queueCount,
                    vk::to_string(qp.queueFlags).c_str(),
                    vk::to_string(video.videoCodecOperations).c_str());
    }

    const vk::DeviceCreateInfo dci{
        .pNext = &d.features2,
        .queueCreateInfoCount = static_cast<uint32_t>(qcis.size()),
        .pQueueCreateInfos = qcis.data(),
        .enabledExtensionCount = static_cast<uint32_t>(d.device_extensions.size()),
        .ppEnabledExtensionNames = d.device_extensions.data()};
    d.device = expect_vk(d.physical.createDevice(dci), "createDevice");

    std::printf("enabled device extensions (%zu):", d.device_extensions.size());
    for (const char* e : d.device_extensions) {
        std::printf(" %s", e);
    }
    std::printf("\ninternally synchronized queues: %s\n", d.internally_synchronized ? "yes" : "no");
}

// Wraps OmaMovie's device in an FFmpeg AVHWDeviceContext without letting FFmpeg create its own.
AVBufferRef* wrap_for_ffmpeg(OwnDevice& d) {
    AVBufferRef* ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VULKAN);
    auto* hw = reinterpret_cast<AVHWDeviceContext*>(ref->data);
    auto* vk_ctx = static_cast<AVVulkanDeviceContext*>(hw->hwctx);
    vk_ctx->get_proc_addr = vkGetInstanceProcAddr;
    vk_ctx->inst = static_cast<VkInstance>(*d.instance);
    vk_ctx->phys_dev = static_cast<VkPhysicalDevice>(*d.physical);
    vk_ctx->act_dev = static_cast<VkDevice>(*d.device);
    vk_ctx->device_features = static_cast<VkPhysicalDeviceFeatures2>(d.features2);
    vk_ctx->enabled_inst_extensions = nullptr;
    vk_ctx->nb_enabled_inst_extensions = 0;
    vk_ctx->enabled_dev_extensions = d.device_extensions.data();
    vk_ctx->nb_enabled_dev_extensions = static_cast<int>(d.device_extensions.size());
    vk_ctx->nb_qf = 0;
    for (const auto& f : d.families) {
        vk_ctx->qf[vk_ctx->nb_qf++] = f;
    }
    vk_ctx->queue_flags = d.internally_synchronized
                              ? VK_DEVICE_QUEUE_CREATE_INTERNALLY_SYNCHRONIZED_BIT_KHR
                              : 0;
    if (const int err = av_hwdevice_ctx_init(ref); err < 0) {
        die("av_hwdevice_ctx_init(vulkan, own device): " + av_err(err));
    }
    return ref;
}

// ------------------------------------------------------------------------- decoding

struct DecodeState {
    AVPixelFormat wanted = AV_PIX_FMT_NONE;
    AVPixelFormat negotiated = AV_PIX_FMT_NONE;
};

AVPixelFormat pick_format(AVCodecContext* ctx, const AVPixelFormat* fmts) {
    auto* st = static_cast<DecodeState*>(ctx->opaque);
    for (const AVPixelFormat* p = fmts; *p != AV_PIX_FMT_NONE; ++p) {
        if (*p == st->wanted) {
            st->negotiated = *p;
            return *p;
        }
    }
    // FFmpeg would silently continue in software; record it so the spike reports it.
    st->negotiated = fmts[0];
    return fmts[0];
}

struct Media {
    AVFormatContext* fmt = nullptr;
    AVCodecContext* dec = nullptr;
    int stream = -1;
    DecodeState state;
};

Media open_media(const char* path, AVBufferRef* hw_device, AVPixelFormat wanted) {
    Media m;
    if (const int err = avformat_open_input(&m.fmt, path, nullptr, nullptr); err < 0) {
        die(std::string("open ") + path + ": " + av_err(err));
    }
    avformat_find_stream_info(m.fmt, nullptr);
    const AVCodec* codec = nullptr;
    m.stream = av_find_best_stream(m.fmt, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (m.stream < 0) {
        die("no video stream");
    }
    // libdav1d is preferred for AV1 but has no hwaccel; hardware decode needs the native decoder.
    if (hw_device != nullptr && codec->id == AV_CODEC_ID_AV1) {
        codec = avcodec_find_decoder_by_name("av1");
    }
    m.dec = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(m.dec, m.fmt->streams[m.stream]->codecpar);
    m.state.wanted = wanted;
    m.dec->opaque = &m.state;
    if (hw_device != nullptr) {
        m.dec->hw_device_ctx = av_buffer_ref(hw_device);
        m.dec->get_format = pick_format;
    }
    if (const int err = avcodec_open2(m.dec, codec, nullptr); err < 0) {
        die("avcodec_open2: " + av_err(err));
    }
    return m;
}

void close_media(Media& m) {
    avcodec_free_context(&m.dec);
    avformat_close_input(&m.fmt);
}

// Calls fn(frame) for up to max_frames decoded frames.
template <typename Fn>
int decode_frames(Media& m, int max_frames, Fn&& fn) {
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    int count = 0;
    bool draining = false;
    while (count < max_frames) {
        if (!draining) {
            const int r = av_read_frame(m.fmt, pkt);
            if (r < 0) {
                draining = true;
                avcodec_send_packet(m.dec, nullptr);
            } else if (pkt->stream_index == m.stream) {
                avcodec_send_packet(m.dec, pkt);
                av_packet_unref(pkt);
            } else {
                av_packet_unref(pkt);
                continue;
            }
        }
        for (;;) {
            const int r = avcodec_receive_frame(m.dec, frame);
            if (r == AVERROR(EAGAIN)) {
                break;
            }
            if (r < 0) {
                av_frame_free(&frame);
                av_packet_free(&pkt);
                return count;
            }
            fn(frame);
            av_frame_unref(frame);
            if (++count >= max_frames) {
                break;
            }
        }
    }
    av_frame_free(&frame);
    av_packet_free(&pkt);
    return count;
}

// ----------------------------------------------------- consuming frames on our own queue

struct Consumer {
    OwnDevice& d;
    vk::raii::Queue queue{nullptr};
    vk::raii::CommandPool pool{nullptr};
    vk::raii::CommandBuffer cmd{nullptr};
    vk::raii::Fence fence{nullptr};
    vk::raii::Buffer readback{nullptr};
    vk::raii::DeviceMemory readback_mem{nullptr};
    static constexpr vk::DeviceSize kReadbackBytes = 64;

    explicit Consumer(OwnDevice& dev) : d(dev) {
        const vk::DeviceQueueInfo2 qi{.flags = d.internally_synchronized
                                                   ? vk::DeviceQueueCreateFlagBits::eInternallySynchronizedKHR
                                                   : vk::DeviceQueueCreateFlags{},
                                      .queueFamilyIndex = d.graphics_family,
                                      .queueIndex = 0};
        queue = d.device.getQueue2(qi);
        pool = expect_vk(d.device.createCommandPool(
                             {.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                              .queueFamilyIndex = d.graphics_family}),
                         "createCommandPool");
        auto cmds = expect_vk(d.device.allocateCommandBuffers(
                                  {.commandPool = *pool,
                                   .level = vk::CommandBufferLevel::ePrimary,
                                   .commandBufferCount = 1}),
                              "allocateCommandBuffers");
        cmd = std::move(cmds.front());
        fence = expect_vk(d.device.createFence({}), "createFence");

        readback = expect_vk(d.device.createBuffer({.size = kReadbackBytes,
                                                    .usage = vk::BufferUsageFlagBits::eTransferDst}),
                             "createBuffer");
        const auto req = readback.getMemoryRequirements();
        const auto mp = d.physical.getMemoryProperties();
        uint32_t type = UINT32_MAX;
        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
            const auto want = vk::MemoryPropertyFlagBits::eHostVisible |
                              vk::MemoryPropertyFlagBits::eHostCoherent;
            if ((req.memoryTypeBits & (1U << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) {
                type = i;
                break;
            }
        }
        readback_mem = expect_vk(
            d.device.allocateMemory({.allocationSize = req.size, .memoryTypeIndex = type}),
            "allocateMemory");
        check_vk(readback.bindMemory(*readback_mem, 0), "bindMemory");
    }

    // Waits on the frame's timeline semaphore, optionally copies the first luma row into a host
    // buffer, transitions the image back and signals sem_value + 1 (the AVVkFrame contract).
    // Returns the luma bytes when copy_luma is set.
    std::vector<uint8_t> consume(AVHWFramesContext* fc, AVVkFrame* vkf, bool copy_luma) {
        auto* vk_fc = static_cast<AVVulkanFramesContext*>(fc->hwctx);
        if (vk_fc->lock_frame) {
            vk_fc->lock_frame(fc, vkf);
        }
        const bool separate_planes = vkf->img[1] != VK_NULL_HANDLE;
        const vk::Image img(vkf->img[0]);
        const vk::ImageAspectFlags aspect = separate_planes ? vk::ImageAspectFlagBits::eColor
                                                            : vk::ImageAspectFlagBits::ePlane0;
        const auto old_layout = static_cast<vk::ImageLayout>(vkf->layout[0]);

        check_vk(cmd.reset(), "reset");
        check_vk(cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit}), "begin");
        // eColor covers every plane of a multi-planar image in a barrier.
        const vk::ImageSubresourceRange range{.aspectMask = vk::ImageAspectFlagBits::eColor,
                                              .levelCount = 1,
                                              .layerCount = 1};
        vk::ImageMemoryBarrier2 to_src{.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                                       .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
                                       .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
                                       .dstAccessMask = vk::AccessFlagBits2::eTransferRead,
                                       .oldLayout = old_layout,
                                       .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                                       .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                       .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                       .image = img,
                                       .subresourceRange = range};
        cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &to_src});
        if (copy_luma) {
            const vk::BufferImageCopy region{
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {.aspectMask = aspect, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
                .imageOffset = {0, 0, 0},
                .imageExtent = {static_cast<uint32_t>(kReadbackBytes), 1, 1}};
            cmd.copyImageToBuffer(img, vk::ImageLayout::eTransferSrcOptimal, *readback, region);
        }
        vk::ImageMemoryBarrier2 back = to_src;
        back.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
        back.srcAccessMask = vk::AccessFlagBits2::eTransferRead;
        back.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
        back.dstAccessMask = vk::AccessFlagBits2::eMemoryRead;
        back.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        back.newLayout = vk::ImageLayout::eGeneral;
        cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &back});
        check_vk(cmd.end(), "end");

        const vk::SemaphoreSubmitInfo wait{.semaphore = vk::Semaphore(vkf->sem[0]),
                                           .value = vkf->sem_value[0],
                                           .stageMask = vk::PipelineStageFlagBits2::eAllCommands};
        const vk::SemaphoreSubmitInfo signal{.semaphore = vk::Semaphore(vkf->sem[0]),
                                             .value = vkf->sem_value[0] + 1,
                                             .stageMask = vk::PipelineStageFlagBits2::eAllCommands};
        const vk::CommandBufferSubmitInfo cbi{.commandBuffer = *cmd};
        const vk::SubmitInfo2 submit{.waitSemaphoreInfoCount = 1,
                                     .pWaitSemaphoreInfos = &wait,
                                     .commandBufferInfoCount = 1,
                                     .pCommandBufferInfos = &cbi,
                                     .signalSemaphoreInfoCount = 1,
                                     .pSignalSemaphoreInfos = &signal};
        // With internally synchronized queues no external lock is needed; otherwise this spike
        // is single-threaded on this queue, which is enough for the experiment.
        check_vk(queue.submit2(submit, *fence), "submit2");
        vkf->sem_value[0] += 1;
        vkf->layout[0] = VK_IMAGE_LAYOUT_GENERAL;
        vkf->access[0] = VK_ACCESS_2_MEMORY_READ_BIT;
        if (vk_fc->unlock_frame) {
            vk_fc->unlock_frame(fc, vkf);
        }
        static_cast<void>(d.device.waitForFences(*fence, vk::True, UINT64_MAX));
        check_vk(d.device.resetFences(*fence), "resetFences");

        std::vector<uint8_t> out;
        if (copy_luma) {
            auto* p = static_cast<const uint8_t*>(expect_vk(readback_mem.mapMemory(0, kReadbackBytes), "mapMemory"));
            out.assign(p, p + kReadbackBytes);
            readback_mem.unmapMemory();
        }
        return out;
    }
};

std::vector<uint8_t> software_first_row(const char* path, size_t n) {
    Media m = open_media(path, nullptr, AV_PIX_FMT_NONE);
    std::vector<uint8_t> row;
    decode_frames(m, 1, [&](AVFrame* f) {
        row.assign(f->data[0], f->data[0] + std::min<size_t>(n, static_cast<size_t>(f->linesize[0])));
    });
    close_media(m);
    return row;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <media> [--mode vaapi|vulkan] [--frames N]\n", argv[0]);
        return 2;
    }
    const char* path = argv[1];
    std::string mode = "vaapi";
    int max_frames = 300;
    for (int i = 2; i + 1 < argc; i += 2) {
        if (std::string_view(argv[i]) == "--mode") {
            mode = argv[i + 1];
        } else if (std::string_view(argv[i]) == "--frames") {
            max_frames = std::atoi(argv[i + 1]);
        }
    }
    av_log_set_level(std::getenv("OMA_AVLOG_DEBUG") ? AV_LOG_DEBUG : AV_LOG_WARNING);

    OwnDevice d;
    create_own_device(d);
    AVBufferRef* vk_dev = nullptr;
    if (std::getenv("OMA_FFMPEG_OWNS_DEVICE")) {
        // Diagnostic only: let FFmpeg create its own device to compare behavior.
        if (const int err = av_hwdevice_ctx_create(&vk_dev, AV_HWDEVICE_TYPE_VULKAN, nullptr, nullptr, 0); err < 0) {
            die("FFmpeg-created Vulkan device: " + av_err(err));
        }
        std::printf("DIAGNOSTIC: using an FFmpeg-created VkDevice\n");
    } else {
        vk_dev = wrap_for_ffmpeg(d);
        std::printf("FFmpeg accepted the OmaMovie-created VkDevice\n");
    }

    AVBufferRef* decode_dev = nullptr;
    AVPixelFormat wanted = AV_PIX_FMT_VULKAN;
    if (mode == "vaapi") {
        if (const int err = av_hwdevice_ctx_create(&decode_dev, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0); err < 0) {
            die("VA-API device: " + av_err(err));
        }
        wanted = AV_PIX_FMT_VAAPI;
    } else if (mode == "vulkan") {
        decode_dev = av_buffer_ref(vk_dev);
    } else {
        die("unknown mode " + mode);
    }

    Media m = open_media(path, decode_dev, wanted);
    Consumer consumer(d);
    AVBufferRef* vk_frames = nullptr; // derived Vulkan frames context (vaapi mode)

    int vk_frames_seen = 0;
    int on_our_queue = 0;
    std::vector<uint8_t> gpu_row;
    std::string drm_info;
    std::string vk_info;
    AVPixelFormat gpu_sw_format = AV_PIX_FMT_NONE;
    const auto t0 = std::chrono::steady_clock::now();
    const double c0 = cpu_seconds();

    const int decoded = decode_frames(m, max_frames, [&](AVFrame* frame) {
        AVFrame* vkframe = nullptr;
        if (frame->format == AV_PIX_FMT_VAAPI) {
            if (vk_frames == nullptr) {
                const int err = av_hwframe_ctx_create_derived(&vk_frames, AV_PIX_FMT_VULKAN, vk_dev,
                                                              frame->hw_frames_ctx, 0);
                if (err < 0) {
                    die("derive Vulkan frames from VA-API: " + av_err(err));
                }
            }
            // Explicit two-step mapping VA-API -> DRM -> Vulkan, to show the DMA-BUF in between.
            AVFrame* drm = av_frame_alloc();
            drm->format = AV_PIX_FMT_DRM_PRIME;
            if (const int err = av_hwframe_map(drm, frame, AV_HWFRAME_MAP_READ); err < 0) {
                die("map VA-API -> DRM: " + av_err(err));
            }
            if (drm_info.empty()) {
                const auto* desc = reinterpret_cast<const AVDRMFrameDescriptor*>(drm->data[0]);
                char buf[256];
                std::snprintf(buf, sizeof buf, "objects=%d layers=%d fourcc=%.4s modifier=0x%llx planes=%d",
                              desc->nb_objects, desc->nb_layers,
                              reinterpret_cast<const char*>(&desc->layers[0].format),
                              static_cast<unsigned long long>(desc->objects[0].format_modifier),
                              desc->layers[0].nb_planes);
                drm_info = buf;
            }
            vkframe = av_frame_alloc();
            vkframe->format = AV_PIX_FMT_VULKAN;
            vkframe->hw_frames_ctx = av_buffer_ref(vk_frames);
            // AV_HWFRAME_MAP_DIRECT makes FFmpeg 9 return EINVAL for VA-API -> Vulkan (also on a
            // device FFmpeg creates itself); without it the mapping still imports the DMA-BUF.
            const int map_flags = std::getenv("OMA_MAP_DIRECT") ? (AV_HWFRAME_MAP_READ | AV_HWFRAME_MAP_DIRECT)
                                                               : AV_HWFRAME_MAP_READ;
            if (const int err = av_hwframe_map(vkframe, frame, map_flags); err < 0) {
                die("map VA-API -> Vulkan: " + av_err(err));
            }
            av_frame_free(&drm);
        } else if (frame->format == AV_PIX_FMT_VULKAN) {
            vkframe = av_frame_clone(frame);
        } else {
            return; // software fallback; counted below
        }

        auto* vkf = reinterpret_cast<AVVkFrame*>(vkframe->data[0]);
        auto* fc = reinterpret_cast<AVHWFramesContext*>(vkframe->hw_frames_ctx->data);
        ++vk_frames_seen;
        if (vk_info.empty()) {
            gpu_sw_format = fc->sw_format;
            const auto* vk_fc = static_cast<AVVulkanFramesContext*>(fc->hwctx);
            char buf[256];
            std::snprintf(buf, sizeof buf, "sw_format=%s images=%d tiling=%s mem_flags=%s sem_value=%llu",
                          av_get_pix_fmt_name(fc->sw_format), vkf->img[1] ? 2 : 1,
                          vk::to_string(static_cast<vk::ImageTiling>(vkf->tiling)).c_str(),
                          vk::to_string(static_cast<vk::MemoryPropertyFlags>(vkf->flags)).c_str(),
                          static_cast<unsigned long long>(vkf->sem_value[0]));
            static_cast<void>(vk_fc);
            vk_info = buf;
        }
        auto row = consumer.consume(fc, vkf, gpu_row.empty());
        if (gpu_row.empty()) {
            gpu_row = std::move(row);
        }
        ++on_our_queue;
        av_frame_free(&vkframe);
    });

    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double cpu = cpu_seconds() - c0;

    std::printf("\nmode: %s\n", mode.c_str());
    std::printf("negotiated decoder format: %s%s\n", av_get_pix_fmt_name(m.state.negotiated),
                m.state.negotiated == wanted ? "" : "  <-- SOFTWARE FALLBACK");
    if (!drm_info.empty()) {
        std::printf("DMA-BUF between VA-API and Vulkan: %s\n", drm_info.c_str());
    }
    if (!vk_info.empty()) {
        std::printf("AVVkFrame on our device: %s\n", vk_info.c_str());
    }
    std::printf("frames decoded: %d, as AVVkFrame: %d, consumed on our queue: %d\n", decoded,
                vk_frames_seen, on_our_queue);
    std::printf("wall %.3f s (%.1f fps incl. per-frame fence wait), cpu %.3f s (%.0f%% of one core)\n",
                wall, decoded / wall, cpu, 100.0 * cpu / wall);

    if (!gpu_row.empty()) {
        const auto sw_row = software_first_row(path, gpu_row.size());
        // P010 keeps 10-bit samples in the high bits of each 16-bit word; FFmpeg's software
        // yuv420p10le keeps them in the low bits. Compare samples, not bytes, for that case.
        const bool p010 = gpu_sw_format == AV_PIX_FMT_P010LE;
        const size_t bytes = std::min(sw_row.size(), gpu_row.size());
        const size_t step = p010 ? 2 : 1;
        size_t equal = 0;
        size_t samples = 0;
        int max_diff = 0;
        for (size_t i = 0; i + step <= bytes; i += step) {
            int g = gpu_row[i];
            int w = sw_row[i];
            if (p010) {
                g = (gpu_row[i] | (gpu_row[i + 1] << 8)) >> 6;
                w = sw_row[i] | (sw_row[i + 1] << 8);
            }
            const int diff = std::abs(w - g);
            equal += diff == 0;
            max_diff = std::max(max_diff, diff);
            ++samples;
        }
        std::printf("first-frame luma check vs software decode: %zu/%zu samples equal, max diff %d\n",
                    equal, samples, max_diff);
    }

    av_buffer_unref(&vk_frames);
    close_media(m);
    av_buffer_unref(&decode_dev);
    av_buffer_unref(&vk_dev);
    for (const char* e : d.device_extensions) {
        std::free(const_cast<char*>(e));
    }
    return vk_frames_seen == decoded && decoded > 0 ? 0 : 1;
}
