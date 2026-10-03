#include "oma/compositor/compositor.hpp"

#include "layer_params.hpp"

#include "oma/base/log.hpp"
#include "oma/gpu/commands.hpp"
#include "oma/gpu/device.hpp"
#include "oma/gpu/resources.hpp"
#include "oma/media/video_frame.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace oma::compositor {

namespace {

// SPIR-V of shaders/composite.comp, compiled by the build (glslc -mfmt=num).
constexpr auto kCompositeSpv = std::to_array<std::uint32_t>({
#include "composite.comp.inc"
});

constexpr std::uint32_t kGroupSize = 16;
constexpr VkFormat kOutputFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr std::size_t kMaxPlanes = 3;

Error vk_failure(VkResult r, std::string what) {
    return {ErrorCode::Internal, Category::Compositor, std::move(what),
            "VkResult " + std::to_string(static_cast<int>(r))};
}

VkFormat plane_format(const media::SampleLayout& layout, int plane) {
    const bool wide = layout.container_bits == 16;
    if (plane > 0 && layout.interleaved_chroma) {
        return wide ? VK_FORMAT_R16G16_UNORM : VK_FORMAT_R8G8_UNORM;
    }
    return wide ? VK_FORMAT_R16_UNORM : VK_FORMAT_R8_UNORM;
}

std::uint32_t plane_width(const media::SampleLayout& layout, std::uint32_t width, int plane) {
    if (plane == 0) {
        return width;
    }
    const std::uint32_t step = 1U << static_cast<unsigned>(layout.chroma_shift_x);
    return (width + step - 1) / step;
}

std::uint32_t plane_height(const media::SampleLayout& layout, std::uint32_t height, int plane) {
    if (plane == 0) {
        return height;
    }
    const std::uint32_t step = 1U << static_cast<unsigned>(layout.chroma_shift_y);
    return (height + step - 1) / step;
}

std::uint32_t texel_bytes(const media::SampleLayout& layout, int plane) {
    const std::uint32_t sample = static_cast<std::uint32_t>(layout.container_bits) / 8;
    return plane > 0 && layout.interleaved_chroma ? sample * 2 : sample;
}

float half_to_float(std::uint16_t h) {
    const std::uint32_t v = h; // no promotion to signed int below
    const std::uint32_t sign = (v & 0x8000U) << 16U;
    std::uint32_t exponent = (v >> 10U) & 0x1FU;
    std::uint32_t mantissa = v & 0x3FFU;
    std::uint32_t bits = 0;
    if (exponent == 0) {
        if (mantissa == 0) {
            bits = sign;
        } else { // subnormal: normalize
            exponent = 127 - 15 + 1;
            while ((mantissa & 0x400U) == 0) {
                mantissa <<= 1U;
                --exponent;
            }
            mantissa &= 0x3FFU;
            bits = sign | (exponent << 23U) | (mantissa << 13U);
        }
    } else if (exponent == 0x1F) {
        bits = sign | 0x7F800000U | (mantissa << 13U);
    } else {
        bits = sign | ((exponent + 127 - 15) << 23U) | (mantissa << 13U);
    }
    float f = 0.0F;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

VkMemoryBarrier2 memory_barrier(VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access,
                                VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access) {
    return {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = src_stage,
            .srcAccessMask = src_access,
            .dstStageMask = dst_stage,
            .dstAccessMask = dst_access};
}

void barrier(VkCommandBuffer cmd, const VkMemoryBarrier2& b) {
    const VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                               .pNext = nullptr,
                               .dependencyFlags = 0,
                               .memoryBarrierCount = 1,
                               .pMemoryBarriers = &b,
                               .bufferMemoryBarrierCount = 0,
                               .pBufferMemoryBarriers = nullptr,
                               .imageMemoryBarrierCount = 0,
                               .pImageMemoryBarriers = nullptr};
    vkCmdPipelineBarrier2(cmd, &dep);
}

// Planes of a software frame copied to the GPU, reused while the size and layout stay the same.
struct Upload {
    media::SampleLayout layout;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::array<gpu::Image, kMaxPlanes> planes;
    std::array<VkDeviceSize, kMaxPlanes> offsets{};
    gpu::Buffer staging;
};

// What one input contributes to this render.
struct BoundInput {
    std::array<VkImageView, kMaxPlanes> views{};
    std::optional<media::GpuAccess> access; // GPU frames: held until the submission is recorded
    media::SampleLayout layout;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

} // namespace

struct VulkanCompositor::Impl {
    const gpu::Device* device = nullptr;
    std::unique_ptr<gpu::CommandRunner> runner;
    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipelineCache cache = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    PFN_vkCmdPushDescriptorSetKHR push_descriptors = nullptr;
    VkDeviceSize params_stride = 0;

    gpu::Image output;
    gpu::Buffer params;
    gpu::Buffer readback;
    std::map<std::size_t, Upload> uploads; // by input index
    std::vector<VkImageView> transient_views;

    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;
    ~Impl() {
        if (device == nullptr) {
            return;
        }
        destroy_transient_views();
        vkDestroySampler(device->device(), sampler, nullptr);
        vkDestroyPipeline(device->device(), pipeline, nullptr);
        vkDestroyPipelineCache(device->device(), cache, nullptr);
        vkDestroyPipelineLayout(device->device(), pipeline_layout, nullptr);
        vkDestroyDescriptorSetLayout(device->device(), set_layout, nullptr);
    }

    void destroy_transient_views() {
        for (VkImageView v : transient_views) {
            vkDestroyImageView(device->device(), v, nullptr);
        }
        transient_views.clear();
    }

    [[nodiscard]] Result<VkImageView> make_view(VkImage image, VkFormat format,
                                                VkImageAspectFlags aspect);
    [[nodiscard]] Result<void> ensure_output(std::uint32_t width, std::uint32_t height);
    [[nodiscard]] Result<void> ensure_params(std::size_t layers);
    [[nodiscard]] Result<Upload*> stage_upload(std::size_t index, const media::VideoFrame& frame);
    [[nodiscard]] Result<BoundInput> bind_gpu_frame(media::VideoFrame& frame);
};

Result<VkImageView> VulkanCompositor::Impl::make_view(VkImage image, VkFormat format,
                                                      VkImageAspectFlags aspect) {
    const VkImageViewCreateInfo info{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                     .pNext = nullptr,
                                     .flags = 0,
                                     .image = image,
                                     .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                     .format = format,
                                     .components = {},
                                     .subresourceRange = {.aspectMask = aspect,
                                                          .baseMipLevel = 0,
                                                          .levelCount = 1,
                                                          .baseArrayLayer = 0,
                                                          .layerCount = 1}};
    VkImageView view = VK_NULL_HANDLE;
    if (const VkResult r = vkCreateImageView(device->device(), &info, nullptr, &view);
        r != VK_SUCCESS) {
        return std::unexpected(vk_failure(r, "cannot create a plane view"));
    }
    transient_views.push_back(view);
    return view;
}

Result<void> VulkanCompositor::Impl::ensure_output(std::uint32_t width, std::uint32_t height) {
    if (output.handle() != VK_NULL_HANDLE && output.desc().width == width &&
        output.desc().height == height) {
        return {};
    }
    auto img = gpu::Image::create(
        *device, {.width = width,
                  .height = height,
                  .format = kOutputFormat,
                  .usage = static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_STORAGE_BIT) |
                           VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT});
    if (!img) {
        return std::unexpected(img.error());
    }
    output = std::move(*img);
    return {};
}

Result<void> VulkanCompositor::Impl::ensure_params(std::size_t layers) {
    const VkDeviceSize needed = params_stride * std::max<VkDeviceSize>(layers, 1);
    if (params.size() >= needed) {
        return {};
    }
    auto buf = gpu::Buffer::create(*device, std::max<VkDeviceSize>(needed, params_stride * 8),
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, gpu::MemoryUse::Upload);
    if (!buf) {
        return std::unexpected(buf.error());
    }
    params = std::move(*buf);
    return {};
}

// Copies a software frame's planes into the staging buffer of its upload slot.
Result<Upload*> VulkanCompositor::Impl::stage_upload(std::size_t index,
                                                     const media::VideoFrame& frame) {
    const media::SampleLayout layout = frame.layout();
    const auto width = static_cast<std::uint32_t>(frame.width());
    const auto height = static_cast<std::uint32_t>(frame.height());
    Upload& up = uploads[index];
    if (up.width != width || up.height != height || !(up.layout == layout) ||
        up.staging.handle() == VK_NULL_HANDLE) {
        up = Upload{};
        up.layout = layout;
        up.width = width;
        up.height = height;
        VkDeviceSize total = 0;
        for (int p = 0; p < layout.planes; ++p) {
            const auto i = static_cast<std::size_t>(p);
            auto img = gpu::Image::create(
                *device, {.width = plane_width(layout, width, p),
                          .height = plane_height(layout, height, p),
                          .format = plane_format(layout, p),
                          .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT});
            if (!img) {
                return std::unexpected(img.error());
            }
            up.planes[i] = std::move(*img);
            up.offsets[i] = total;
            const VkDeviceSize bytes = static_cast<VkDeviceSize>(plane_width(layout, width, p)) *
                                       plane_height(layout, height, p) * texel_bytes(layout, p);
            total += (bytes + 15) & ~VkDeviceSize{15};
        }
        auto staging = gpu::Buffer::create(*device, total, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                           gpu::MemoryUse::Upload);
        if (!staging) {
            return std::unexpected(staging.error());
        }
        up.staging = std::move(*staging);
    }
    auto* base = static_cast<std::uint8_t*>(up.staging.mapped());
    for (int p = 0; p < layout.planes; ++p) {
        const auto i = static_cast<std::size_t>(p);
        const std::size_t row_bytes =
            static_cast<std::size_t>(plane_width(layout, width, p)) * texel_bytes(layout, p);
        const std::uint32_t rows = plane_height(layout, height, p);
        const auto src = frame.plane(p);
        const auto stride = static_cast<std::size_t>(frame.stride(p));
        if (src.size() < (stride * (rows - 1)) + row_bytes) {
            return make_error(ErrorCode::InvalidData, Category::Compositor,
                              "frame plane is too small");
        }
        for (std::uint32_t y = 0; y < rows; ++y) {
            std::memcpy(base + up.offsets[i] + (y * row_bytes), src.data() + (y * stride),
                        row_bytes);
        }
    }
    up.staging.flush();
    return &up;
}

Result<BoundInput> VulkanCompositor::Impl::bind_gpu_frame(media::VideoFrame& frame) {
    BoundInput bound;
    bound.layout = frame.layout();
    bound.width = static_cast<std::uint32_t>(frame.width());
    bound.height = static_cast<std::uint32_t>(frame.height());
    auto access = frame.acquire_gpu();
    if (!access) {
        return std::unexpected(access.error());
    }
    const media::GpuImages& images = access->images();
    const bool multiplanar = images.image_count == 1 && bound.layout.planes > 1;
    for (int p = 0; p < bound.layout.planes && std::cmp_less(p, kMaxPlanes); ++p) {
        const auto i = static_cast<std::size_t>(p);
        Result<VkImageView> view =
            multiplanar
                ? make_view(images.images[0], plane_format(bound.layout, p),
                            static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_PLANE_0_BIT << i))
                : make_view(images.images[i],
                            images.formats[i] != VK_FORMAT_UNDEFINED
                                ? images.formats[i]
                                : plane_format(bound.layout, p),
                            VK_IMAGE_ASPECT_COLOR_BIT);
        if (!view) {
            return std::unexpected(view.error());
        }
        bound.views[i] = *view;
    }
    bound.access = std::move(*access);
    return bound;
}

VulkanCompositor::VulkanCompositor(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
VulkanCompositor::~VulkanCompositor() = default;

Result<std::unique_ptr<VulkanCompositor>> VulkanCompositor::create(const gpu::Device& device) {
    auto impl = std::make_unique<Impl>();
    impl->device = &device;

    impl->push_descriptors = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(
        vkGetDeviceProcAddr(device.device(), "vkCmdPushDescriptorSetKHR"));
    if (impl->push_descriptors == nullptr) {
        impl->push_descriptors = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(
            vkGetDeviceProcAddr(device.device(), "vkCmdPushDescriptorSet"));
    }
    if (impl->push_descriptors == nullptr) {
        return make_error(ErrorCode::Unsupported, Category::Compositor,
                          "the device has no push descriptors");
    }
    auto runner = gpu::CommandRunner::create(device, device.graphics_family());
    if (!runner) {
        return std::unexpected(runner.error());
    }
    impl->runner = std::make_unique<gpu::CommandRunner>(std::move(*runner));

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(device.physical_device(), &props);
    const VkDeviceSize align =
        std::max<VkDeviceSize>(props.limits.minUniformBufferOffsetAlignment, 16);
    impl->params_stride = (sizeof(LayerParams) + align - 1) / align * align;

    std::array<VkDescriptorSetLayoutBinding, 5> bindings{};
    for (std::uint32_t i = 0; i < bindings.size(); ++i) {
        bindings[i] = {.binding = i,
                       .descriptorType = i == 0   ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                         : i == 4 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                                  : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       .descriptorCount = 1,
                       .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                       .pImmutableSamplers = nullptr};
    }
    const VkDescriptorSetLayoutCreateInfo set_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
        .bindingCount = static_cast<std::uint32_t>(bindings.size()),
        .pBindings = bindings.data()};
    if (const VkResult r =
            vkCreateDescriptorSetLayout(device.device(), &set_info, nullptr, &impl->set_layout);
        r != VK_SUCCESS) {
        return std::unexpected(vk_failure(r, "cannot create the descriptor layout"));
    }
    const VkPipelineLayoutCreateInfo layout_info{.sType =
                                                     VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                 .pNext = nullptr,
                                                 .flags = 0,
                                                 .setLayoutCount = 1,
                                                 .pSetLayouts = &impl->set_layout,
                                                 .pushConstantRangeCount = 0,
                                                 .pPushConstantRanges = nullptr};
    if (const VkResult r =
            vkCreatePipelineLayout(device.device(), &layout_info, nullptr, &impl->pipeline_layout);
        r != VK_SUCCESS) {
        return std::unexpected(vk_failure(r, "cannot create the pipeline layout"));
    }
    // TODO(M3): persist the cache under $XDG_CACHE_HOME/omamovie (CLAUDE.md §9.5).
    const VkPipelineCacheCreateInfo cache_info{.sType =
                                                   VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
                                               .pNext = nullptr,
                                               .flags = 0,
                                               .initialDataSize = 0,
                                               .pInitialData = nullptr};
    if (const VkResult r =
            vkCreatePipelineCache(device.device(), &cache_info, nullptr, &impl->cache);
        r != VK_SUCCESS) {
        return std::unexpected(vk_failure(r, "cannot create the pipeline cache"));
    }

    const VkShaderModuleCreateInfo module_info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                               .pNext = nullptr,
                                               .flags = 0,
                                               .codeSize = sizeof(kCompositeSpv),
                                               .pCode = kCompositeSpv.data()};
    VkShaderModule module = VK_NULL_HANDLE;
    if (const VkResult r = vkCreateShaderModule(device.device(), &module_info, nullptr, &module);
        r != VK_SUCCESS) {
        return std::unexpected(vk_failure(r, "cannot load the compositor shader"));
    }
    const VkComputePipelineCreateInfo pipe_info{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .pNext = nullptr,
                  .flags = 0,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                  .module = module,
                  .pName = "main",
                  .pSpecializationInfo = nullptr},
        .layout = impl->pipeline_layout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1};
    const VkResult made = vkCreateComputePipelines(device.device(), impl->cache, 1, &pipe_info,
                                                   nullptr, &impl->pipeline);
    vkDestroyShaderModule(device.device(), module, nullptr);
    if (made != VK_SUCCESS) {
        return std::unexpected(vk_failure(made, "cannot create the compositor pipeline"));
    }

    const VkSamplerCreateInfo sampler_info{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                           .pNext = nullptr,
                                           .flags = 0,
                                           .magFilter = VK_FILTER_NEAREST,
                                           .minFilter = VK_FILTER_NEAREST,
                                           .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                           .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                           .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                           .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                           .mipLodBias = 0.0F,
                                           .anisotropyEnable = VK_FALSE,
                                           .maxAnisotropy = 1.0F,
                                           .compareEnable = VK_FALSE,
                                           .compareOp = VK_COMPARE_OP_ALWAYS,
                                           .minLod = 0.0F,
                                           .maxLod = 0.0F,
                                           .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                                           .unnormalizedCoordinates = VK_FALSE};
    if (const VkResult r = vkCreateSampler(device.device(), &sampler_info, nullptr, &impl->sampler);
        r != VK_SUCCESS) {
        return std::unexpected(vk_failure(r, "cannot create the sampler"));
    }
    return std::unique_ptr<VulkanCompositor>(new VulkanCompositor(std::move(impl)));
}

Result<void> VulkanCompositor::render(const RenderGraph& graph,
                                      std::span<const LayerInput> inputs) {
    Impl& d = *impl_;
    if (auto valid = validate(graph, inputs.size()); !valid) {
        return std::unexpected(valid.error());
    }
    if (auto r = d.ensure_output(graph.width, graph.height); !r) {
        return r;
    }
    if (auto r = d.ensure_params(graph.layers.size()); !r) {
        return r;
    }

    // Bind every input used by a layer: GPU frames by reference, software frames staged.
    std::map<std::size_t, BoundInput> bound;
    std::vector<const Upload*> uploads;
    for (const Layer& layer : graph.layers) {
        if (bound.contains(layer.input)) {
            continue;
        }
        media::VideoFrame* frame = inputs[layer.input].frame;
        if (frame == nullptr) {
            d.destroy_transient_views();
            return make_error(ErrorCode::InvalidArgument, Category::Compositor,
                              "layer input without a frame");
        }
        Result<BoundInput> b =
            std::unexpected(Error(ErrorCode::Internal, Category::Compositor, ""));
        if (frame->on_gpu()) {
            b = d.bind_gpu_frame(*frame);
        } else {
            const auto staged = d.stage_upload(layer.input, *frame);
            if (staged) {
                BoundInput in;
                in.layout = (*staged)->layout;
                in.width = (*staged)->width;
                in.height = (*staged)->height;
                for (int p = 0; p < in.layout.planes; ++p) {
                    in.views[static_cast<std::size_t>(p)] =
                        (*staged)->planes[static_cast<std::size_t>(p)].view();
                }
                uploads.push_back(*staged);
                b = std::move(in);
            } else {
                b = std::unexpected(staged.error());
            }
        }
        if (!b) {
            d.destroy_transient_views();
            return std::unexpected(b.error());
        }
        bound.emplace(layer.input, std::move(*b));
    }

    // Per-layer parameters.
    struct Draw {
        std::size_t input;
        VkDeviceSize offset;
        std::array<std::int32_t, 4> region;
    };
    std::vector<Draw> draws;
    auto* params = static_cast<std::uint8_t*>(d.params.mapped());
    for (const Layer& layer : graph.layers) {
        const BoundInput& in = bound.at(layer.input);
        const auto prepared = prepare_layer(layer, inputs[layer.input], in.layout, in.width,
                                            in.height, graph.width, graph.height);
        if (!prepared) {
            d.destroy_transient_views();
            return std::unexpected(prepared.error());
        }
        if (!prepared->visible) {
            continue;
        }
        const VkDeviceSize offset = draws.size() * d.params_stride;
        std::memcpy(params + offset, &prepared->params, sizeof(LayerParams));
        draws.push_back(
            {.input = layer.input, .offset = offset, .region = prepared->params.region});
    }
    d.params.flush();

    std::vector<gpu::SemaphoreSubmit> waits;
    std::vector<gpu::SemaphoreSubmit> signals;
    for (const auto& [index, in] : bound) {
        if (!in.access) {
            continue;
        }
        const media::GpuImages& img = in.access->images();
        for (std::uint32_t i = 0; i < img.image_count; ++i) {
            if (img.semaphores[i] != VK_NULL_HANDLE) {
                waits.push_back({.semaphore = img.semaphores[i], .value = img.wait_values[i]});
                signals.push_back({.semaphore = img.semaphores[i], .value = img.signal_values[i]});
            }
        }
    }

    const auto bg = premultiplied_background(graph);
    const auto ran = d.runner->run(
        [&](VkCommandBuffer cmd) {
            gpu::transition(cmd, d.output.handle(), VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_GENERAL);
            const VkClearColorValue clear{.float32 = {bg[0], bg[1], bg[2], bg[3]}};
            const VkImageSubresourceRange range{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                .baseMipLevel = 0,
                                                .levelCount = 1,
                                                .baseArrayLayer = 0,
                                                .layerCount = 1};
            vkCmdClearColorImage(cmd, d.output.handle(), VK_IMAGE_LAYOUT_GENERAL, &clear, 1,
                                 &range);

            for (const Upload* up : uploads) {
                for (int p = 0; p < up->layout.planes; ++p) {
                    const auto i = static_cast<std::size_t>(p);
                    gpu::transition(cmd, up->planes[i].handle(), VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                    const VkBufferImageCopy copy{
                        .bufferOffset = up->offsets[i],
                        .bufferRowLength = 0,
                        .bufferImageHeight = 0,
                        .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                             .mipLevel = 0,
                                             .baseArrayLayer = 0,
                                             .layerCount = 1},
                        .imageOffset = {.x = 0, .y = 0, .z = 0},
                        .imageExtent = {.width = up->planes[i].desc().width,
                                        .height = up->planes[i].desc().height,
                                        .depth = 1}};
                    vkCmdCopyBufferToImage(cmd, up->staging.handle(), up->planes[i].handle(),
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
                    gpu::transition(cmd, up->planes[i].handle(),
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
                }
            }
            for (const auto& [index, in] : bound) {
                if (!in.access) {
                    continue;
                }
                const media::GpuImages& img = in.access->images();
                for (std::uint32_t i = 0; i < img.image_count; ++i) {
                    gpu::transition(cmd, img.images[i], img.layouts[i], VK_IMAGE_LAYOUT_GENERAL);
                }
            }
            barrier(cmd, memory_barrier(
                             VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                             VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, d.pipeline);
            for (const Draw& draw : draws) {
                const BoundInput& in = bound.at(draw.input);
                const VkDescriptorImageInfo target{.sampler = VK_NULL_HANDLE,
                                                   .imageView = d.output.view(),
                                                   .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
                std::array<VkDescriptorImageInfo, kMaxPlanes> planes{};
                for (std::size_t p = 0; p < kMaxPlanes; ++p) {
                    // Unused planes alias plane 1 so every binding stays valid.
                    planes[p] = {.sampler = d.sampler,
                                 .imageView =
                                     in.views[p] != VK_NULL_HANDLE ? in.views[p] : in.views[1],
                                 .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
                }
                const VkDescriptorBufferInfo ubo{.buffer = d.params.handle(),
                                                 .offset = draw.offset,
                                                 .range = sizeof(LayerParams)};
                std::array<VkWriteDescriptorSet, 5> writes{};
                for (std::uint32_t b = 0; b < writes.size(); ++b) {
                    writes[b] = {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .pNext = nullptr,
                        .dstSet = VK_NULL_HANDLE,
                        .dstBinding = b,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType = b == 0   ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                          : b == 4 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                                   : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .pImageInfo = b == 0 ? &target : (b == 4 ? nullptr : &planes[b - 1]),
                        .pBufferInfo = b == 4 ? &ubo : nullptr,
                        .pTexelBufferView = nullptr};
                }
                d.push_descriptors(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, d.pipeline_layout, 0,
                                   static_cast<std::uint32_t>(writes.size()), writes.data());
                const auto w = static_cast<std::uint32_t>(draw.region[2] - draw.region[0]);
                const auto h = static_cast<std::uint32_t>(draw.region[3] - draw.region[1]);
                vkCmdDispatch(cmd, (w + kGroupSize - 1) / kGroupSize,
                              (h + kGroupSize - 1) / kGroupSize, 1);
                // The next layer reads what this one wrote.
                barrier(cmd,
                        memory_barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                       VK_ACCESS_2_SHADER_WRITE_BIT,
                                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                                           VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                       VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT |
                                           VK_ACCESS_2_MEMORY_READ_BIT));
            }
        },
        waits, signals);

    for (auto& [index, in] : bound) {
        if (in.access && ran) {
            in.access->commit(VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_2_SHADER_READ_BIT);
        }
    }
    bound.clear(); // releases the frame locks
    d.destroy_transient_views();
    return ran;
}

const gpu::Image* VulkanCompositor::output() const noexcept {
    return impl_->output.handle() != VK_NULL_HANDLE ? &impl_->output : nullptr;
}

Result<RgbaImage> VulkanCompositor::read_output() {
    Impl& d = *impl_;
    if (d.output.handle() == VK_NULL_HANDLE) {
        return make_error(ErrorCode::InvalidArgument, Category::Compositor, "nothing rendered yet");
    }
    const std::uint32_t w = d.output.desc().width;
    const std::uint32_t h = d.output.desc().height;
    const VkDeviceSize bytes = static_cast<VkDeviceSize>(w) * h * 4 * sizeof(std::uint16_t);
    if (d.readback.size() < bytes) {
        auto buf = gpu::Buffer::create(*d.device, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       gpu::MemoryUse::Readback);
        if (!buf) {
            return std::unexpected(buf.error());
        }
        d.readback = std::move(*buf);
    }
    const auto ran = d.runner->run([&](VkCommandBuffer cmd) {
        const VkBufferImageCopy copy{.bufferOffset = 0,
                                     .bufferRowLength = 0,
                                     .bufferImageHeight = 0,
                                     .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                          .mipLevel = 0,
                                                          .baseArrayLayer = 0,
                                                          .layerCount = 1},
                                     .imageOffset = {.x = 0, .y = 0, .z = 0},
                                     .imageExtent = {.width = w, .height = h, .depth = 1}};
        vkCmdCopyImageToBuffer(cmd, d.output.handle(), VK_IMAGE_LAYOUT_GENERAL, d.readback.handle(),
                               1, &copy);
        barrier(cmd, memory_barrier(VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT));
    });
    if (!ran) {
        return std::unexpected(ran.error());
    }
    d.readback.invalidate();
    RgbaImage out{
        .width = w, .height = h, .pixels = std::vector<float>(static_cast<std::size_t>(w) * h * 4)};
    const auto* halves = static_cast<const std::uint16_t*>(d.readback.mapped());
    for (std::size_t i = 0; i < out.pixels.size(); ++i) {
        out.pixels[i] = half_to_float(halves[i]);
    }
    return out;
}

} // namespace oma::compositor
