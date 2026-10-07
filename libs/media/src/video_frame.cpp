#include "oma/media/video_frame.hpp"

#include "video_frame_impl.hpp"

#include "oma/gpu/device.hpp"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include <array>
#include <climits>
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
    auto* impl = static_cast<VideoFrame::Impl*>(frame_);
    if (impl->imported) {
        // Foreign images go back to their exporter after each use: GENERAL again.
        for (std::uint32_t i = 0; i < images_.image_count; ++i) {
            impl->imported->values[i] = images_.signal_values[i];
            images_.wait_values[i] = images_.signal_values[i];
            images_.signal_values[i] += 1;
        }
        return;
    }
    auto* vkf = reinterpret_cast<AVVkFrame*>(impl->frame->data[0]);
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
    auto* impl = static_cast<VideoFrame::Impl*>(std::exchange(frame_, nullptr));
    if (impl->imported) {
        impl->imported->lock.unlock();
        return;
    }
    const AVFrame* f = impl->frame.get();
    AVHWFramesContext* fc = frames_of(*f);
    const auto* vk_fc = static_cast<const AVVulkanFramesContext*>(fc->hwctx);
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
    return impl_->frame->format == AV_PIX_FMT_VULKAN || impl_->imported != nullptr;
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

SampleLayout VideoFrame::layout() const noexcept {
    SampleLayout l;
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(impl_->layout);
    if (desc == nullptr) {
        return l;
    }
    l.planes = plane_count();
    l.bit_depth = desc->comp[0].depth;
    l.container_bits = desc->comp[0].step * 8;
    l.lsb_shift = desc->comp[0].shift;
    l.chroma_shift_x = desc->log2_chroma_w;
    l.chroma_shift_y = desc->log2_chroma_h;
    l.yuv = (desc->flags & AV_PIX_FMT_FLAG_RGB) == 0;
    l.alpha = (desc->flags & AV_PIX_FMT_FLAG_ALPHA) != 0;
    l.interleaved_chroma =
        l.yuv && desc->nb_components >= 3 && desc->comp[1].plane == desc->comp[2].plane;
    if (l.interleaved_chroma) {
        l.container_bits = desc->comp[1].step * 4; // two samples per chroma step
    }
    return l;
}

Result<VideoFrame> VideoFrame::from_rgba(int width, int height, std::span<const std::uint8_t> rgba,
                                         int stride) {
    constexpr int kMaxSide = 16384;
    if (width <= 0 || height <= 0 || width > kMaxSide || height > kMaxSide || stride < width * 4 ||
        rgba.size() < static_cast<std::size_t>(stride) * static_cast<std::size_t>(height - 1) +
                          static_cast<std::size_t>(width) * 4) {
        return make_error(ErrorCode::InvalidArgument, Category::Media, "invalid RGBA picture");
    }
    ff::FramePtr frame(av_frame_alloc());
    if (!frame) {
        return make_error(ErrorCode::Internal, Category::Media, "cannot allocate a frame");
    }
    frame->format = AV_PIX_FMT_GBRAP;
    frame->width = width;
    frame->height = height;
    if (const int err = av_frame_get_buffer(frame.get(), 0); err < 0) {
        return std::unexpected(ff::av_error(err, Category::Media, "cannot allocate frame planes"));
    }
    // gbrap planes: 0 G, 1 B, 2 R, 3 A.
    constexpr std::array<int, 4> kSourceChannel{1, 2, 0, 3};
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* row = rgba.data() + static_cast<std::ptrdiff_t>(y) * stride;
        for (std::size_t plane = 0; plane < kSourceChannel.size(); ++plane) {
            std::uint8_t* out =
                frame->data[plane] + static_cast<std::ptrdiff_t>(y) * frame->linesize[plane];
            const int channel = kSourceChannel[plane];
            for (int x = 0; x < width; ++x)
                out[x] = row[(x * 4) + channel];
        }
    }
    auto impl = std::make_unique<Impl>();
    impl->frame = std::move(frame);
    impl->layout = AV_PIX_FMT_GBRAP;
    impl->path = DecodePath::Software;
    return VideoFrame(std::move(impl));
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

Result<void> VideoFrame::copy_rgba(std::span<std::uint8_t> destination,
                                   int destination_stride) const {
    const AVFrame& frame = *impl_->frame;
    if (on_gpu() || frame.width <= 0 || frame.height <= 0 || frame.width > INT_MAX / 4 ||
        destination_stride < frame.width * 4 ||
        destination.size() <
            static_cast<std::size_t>(destination_stride) * static_cast<std::size_t>(frame.height)) {
        return make_error(ErrorCode::InvalidArgument, Category::Decode,
                          "invalid software RGBA preview destination");
    }
    auto* scale = sws_getContext(
        frame.width, frame.height, static_cast<AVPixelFormat>(frame.format), frame.width,
        frame.height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (scale == nullptr) {
        return make_error(ErrorCode::Unsupported, Category::Decode,
                          "cannot convert this video pixel format to RGBA");
    }
    // swscale defaults to BT.601 limited range; use the stream's matrix and range, resolving
    // unspecified values as the compositor does (BT.709 above 576 lines, limited range).
    AVColorSpace matrix = frame.colorspace;
    if (matrix == AVCOL_SPC_UNSPECIFIED || matrix == AVCOL_SPC_RESERVED) {
        matrix = frame.height > 576 ? AVCOL_SPC_BT709 : AVCOL_SPC_SMPTE170M;
    }
    const int* coefficients = sws_getCoefficients(matrix);
    sws_setColorspaceDetails(scale, coefficients, frame.color_range == AVCOL_RANGE_JPEG ? 1 : 0,
                             coefficients, 1, 0, 1 << 16, 1 << 16);
    const std::array<std::uint8_t*, 4> data{destination.data(), nullptr, nullptr, nullptr};
    const std::array<int, 4> linesize{destination_stride, 0, 0, 0};
    const int rows =
        sws_scale(scale, frame.data, frame.linesize, 0, frame.height, data.data(), linesize.data());
    sws_freeContext(scale);
    if (rows != frame.height) {
        return make_error(ErrorCode::Internal, Category::Decode, "RGBA preview conversion failed");
    }
    return {};
}

Result<GpuAccess> VideoFrame::acquire_gpu() {
    if (!on_gpu()) {
        return make_error(ErrorCode::Unsupported, Category::Decode,
                          "frame is in system memory, not on the GPU");
    }
    if (ImportedFrame* imp = impl_->imported.get()) {
        imp->lock.lock(); // unlocked by GpuAccess::release
        GpuImages images;
        images.foreign = true;
        images.image_count = imp->count;
        for (std::uint32_t i = 0; i < imp->count; ++i) {
            images.images[i] = imp->images[i].handle();
            images.formats[i] = imp->images[i].format();
            images.layouts[i] = VK_IMAGE_LAYOUT_GENERAL;
            images.semaphores[i] = imp->semaphores[i];
            images.wait_values[i] = imp->values[i];
            images.signal_values[i] = imp->values[i] + 1;
        }
        return GpuAccess(impl_.get(), images);
    }
    const AVFrame* f = impl_->frame.get();
    AVHWFramesContext* fc = frames_of(*f);
    const auto* vk_fc = static_cast<const AVVulkanFramesContext*>(fc->hwctx);
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
    // One image per plane (VA-API import): FFmpeg reports the multi-planar format of the whole
    // frame for image 0, but each image holds one plane; views need the plane's own format.
    if (images.image_count > 1) {
        const SampleLayout l = layout();
        const bool wide = l.container_bits > 8;
        for (std::uint32_t i = 0; i < images.image_count; ++i) {
            const bool pair = i > 0 && l.interleaved_chroma;
            images.formats[i] = wide ? (pair ? VK_FORMAT_R16G16_UNORM : VK_FORMAT_R16_UNORM)
                                     : (pair ? VK_FORMAT_R8G8_UNORM : VK_FORMAT_R8_UNORM);
        }
    }
    return GpuAccess(impl_.get(), images);
}

ImportedFrame::~ImportedFrame() {
    // The images may still be read by a submitted composite: wait for the last values the
    // consumers signaled before the memory goes back to VA-API.
    if (device == nullptr) {
        return;
    }
    VkDevice dev = device->device();
    if (count > 0) {
        const VkSemaphoreWaitInfo wait{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                                       .pNext = nullptr,
                                       .flags = 0,
                                       .semaphoreCount = count,
                                       .pSemaphores = semaphores.data(),
                                       .pValues = values.data()};
        (void)vkWaitSemaphores(dev, &wait, UINT64_MAX);
    }
    for (std::uint32_t i = 0; i < count; ++i) {
        vkDestroySemaphore(dev, semaphores[i], nullptr);
    }
}

} // namespace oma::media
