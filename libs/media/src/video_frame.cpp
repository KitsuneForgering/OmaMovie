#include "oma/media/video_frame.hpp"

#include "video_frame_impl.hpp"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

#include <array>
#include <cstddef>
#include <utility>

namespace oma::media {

namespace {

AVHWFramesContext* frames_of(const AVFrame& f) {
    return reinterpret_cast<AVHWFramesContext*>(f.hw_frames_ctx->data);
}

} // namespace

std::string_view to_string(DecodePath path) noexcept {
    switch (path) {
    case DecodePath::Software:
        return "software";
    case DecodePath::VaapiToVulkan:
        return "VA-API to Vulkan";
    case DecodePath::VulkanVideo:
        return "Vulkan Video";
    }
    return "unknown";
}

// ------------------------------------------------------------------------------- GpuAccess

GpuAccess::GpuAccess(void* frame, const GpuImages& images) noexcept
    : frame_(frame), images_(images) {}

GpuAccess::GpuAccess(GpuAccess&& other) noexcept
    : frame_(std::exchange(other.frame_, nullptr)), images_(other.images_) {}

GpuAccess& GpuAccess::operator=(GpuAccess&& other) noexcept {
    if (this != &other) {
        release();
        frame_ = std::exchange(other.frame_, nullptr);
        images_ = other.images_;
    }
    return *this;
}

GpuAccess::~GpuAccess() {
    release();
}

void GpuAccess::commit(VkImageLayout layout, VkAccessFlags2 access) noexcept {
    if (frame_ == nullptr) {
        return;
    }
    auto* f = static_cast<AVFrame*>(frame_);
    auto* vkf = reinterpret_cast<AVVkFrame*>(f->data[0]);
    for (std::uint32_t i = 0; i < images_.image_count; ++i) {
        vkf->sem_value[i] = images_.signal_values[i];
        vkf->layout[i] = layout;
        vkf->access[i] = static_cast<VkAccessFlagBits2>(access);
        images_.wait_values[i] = images_.signal_values[i];
        images_.signal_values[i] += 1;
        images_.layouts[i] = layout;
    }
}

void GpuAccess::release() noexcept {
    if (frame_ == nullptr) {
        return;
    }
    auto* f = static_cast<AVFrame*>(std::exchange(frame_, nullptr));
    AVHWFramesContext* fc = frames_of(*f);
    auto* vk_fc = static_cast<AVVulkanFramesContext*>(fc->hwctx);
    if (vk_fc->unlock_frame != nullptr) {
        vk_fc->unlock_frame(fc, reinterpret_cast<AVVkFrame*>(f->data[0]));
    }
}

// ------------------------------------------------------------------------------ VideoFrame

VideoFrame::VideoFrame(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
VideoFrame::VideoFrame(VideoFrame&& other) noexcept = default;
VideoFrame& VideoFrame::operator=(VideoFrame&& other) noexcept = default;
VideoFrame::~VideoFrame() = default;

std::optional<RationalTime> VideoFrame::pts() const noexcept {
    return impl_->pts;
}
std::optional<RationalTime> VideoFrame::duration() const noexcept {
    return impl_->duration;
}
int VideoFrame::width() const noexcept {
    return impl_->frame->width;
}
int VideoFrame::height() const noexcept {
    return impl_->frame->height;
}
DecodePath VideoFrame::path() const noexcept {
    return impl_->path;
}
bool VideoFrame::on_gpu() const noexcept {
    return impl_->frame->format == AV_PIX_FMT_VULKAN;
}

std::string_view VideoFrame::pixel_format() const noexcept {
    const char* name = av_get_pix_fmt_name(impl_->layout);
    return name != nullptr ? name : "";
}

int VideoFrame::bit_depth() const noexcept {
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(impl_->layout);
    return desc != nullptr ? desc->comp[0].depth : 0;
}

int VideoFrame::plane_count() const noexcept {
    const int n = av_pix_fmt_count_planes(impl_->layout);
    return n > 0 ? n : 0;
}

std::span<const std::uint8_t> VideoFrame::plane(int index) const noexcept {
    const AVFrame& f = *impl_->frame;
    if (on_gpu() || index < 0 || index >= plane_count() || f.data[index] == nullptr) {
        return {};
    }
    std::array<std::size_t, 4> sizes{};
    std::array<std::ptrdiff_t, 4> linesizes{};
    for (std::size_t i = 0; i < linesizes.size(); ++i) {
        linesizes[i] = f.linesize[i];
    }
    if (av_image_fill_plane_sizes(sizes.data(), impl_->layout, f.height, linesizes.data()) < 0) {
        return {};
    }
    return {f.data[index], sizes[static_cast<std::size_t>(index)]};
}

int VideoFrame::stride(int index) const noexcept {
    if (on_gpu() || index < 0 || index >= plane_count()) {
        return 0;
    }
    return impl_->frame->linesize[index];
}

Result<GpuAccess> VideoFrame::acquire_gpu() {
    if (!on_gpu()) {
        return make_error(ErrorCode::Unsupported, Category::Decode,
                          "frame is in system memory, not on the GPU");
    }
    AVFrame* f = impl_->frame.get();
    AVHWFramesContext* fc = frames_of(*f);
    auto* vk_fc = static_cast<AVVulkanFramesContext*>(fc->hwctx);
    auto* vkf = reinterpret_cast<AVVkFrame*>(f->data[0]);
    if (vk_fc->lock_frame != nullptr) {
        vk_fc->lock_frame(fc, vkf);
    }

    GpuImages images;
    const VkFormat* fallback_formats = av_vkfmt_from_pixfmt(fc->sw_format);
    for (std::size_t i = 0; i < kMaxFrameImages && vkf->img[i] != VK_NULL_HANDLE; ++i) {
        images.images[i] = vkf->img[i];
        images.formats[i] =
            vk_fc->format[i] != VK_FORMAT_UNDEFINED
                ? vk_fc->format[i]
                : (fallback_formats != nullptr ? fallback_formats[i] : VK_FORMAT_UNDEFINED);
        images.layouts[i] = vkf->layout[i];
        images.semaphores[i] = vkf->sem[i];
        images.wait_values[i] = vkf->sem_value[i];
        images.signal_values[i] = vkf->sem_value[i] + 1;
        images.image_count = static_cast<std::uint32_t>(i + 1);
    }
    return GpuAccess(f, images);
}

} // namespace oma::media
