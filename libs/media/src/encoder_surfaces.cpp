#include "encoder_surfaces_detail.hpp"

extern "C" {
#include <libavutil/hwcontext_drm.h>
#include <libavutil/pixdesc.h>
}

#include <array>
#include <cstdint>

namespace oma::media {

EncoderSurfaces::EncoderSurfaces(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
EncoderSurfaces::~EncoderSurfaces() = default;

Result<EncoderSurface> EncoderSurfaces::acquire() {
    Impl& s = *impl_;
    std::shared_ptr<AVFrame> frame(av_frame_alloc(), [](AVFrame* f) { av_frame_free(&f); });
    if (!frame)
        return make_error(ErrorCode::Internal, Category::Encode, "cannot allocate a frame");
    if (const int err = av_hwframe_get_buffer(s.frames.get(), frame.get(), 0); err < 0)
        return std::unexpected(ff::av_error(err, Category::Encode, "no free VA-API surface"));
    // Exporting the surface waits for the encoder to finish with it (vaSyncSurface) and names
    // its planes; the import itself is made once per surface and kept.
    ff::FramePtr drm(av_frame_alloc());
    if (!drm)
        return make_error(ErrorCode::Internal, Category::Encode, "cannot allocate a frame");
    drm->format = AV_PIX_FMT_DRM_PRIME;
    if (const int err =
            av_hwframe_map(drm.get(), frame.get(), AV_HWFRAME_MAP_READ | AV_HWFRAME_MAP_WRITE);
        err < 0)
        return std::unexpected(
            ff::av_error(err, Category::Encode, "cannot export a VA-API surface"));
    const auto id = reinterpret_cast<std::uintptr_t>(frame->data[3]);
    auto planes = s.imported.find(id);
    if (planes == s.imported.end()) {
        const auto* desc = reinterpret_cast<const AVDRMFrameDescriptor*>(drm->data[0]);
        auto made = import_nv12(*s.device, *desc, frame->width, frame->height);
        if (!made)
            return std::unexpected(made.error());
        planes = s.imported.emplace(id, std::move(*made)).first;
    }
    return EncoderSurface{.luma = planes->second[0].handle(),
                          .luma_view = planes->second[0].view(),
                          .cbcr = planes->second[1].handle(),
                          .cbcr_view = planes->second[1].view(),
                          .frame = std::move(frame)};
}

Result<std::array<gpu::DmaBufImage, 2>>
import_nv12(const gpu::Device& device, const AVDRMFrameDescriptor& desc, int width, int height) {
    // FFmpeg exports NV12 as two single-plane layers (R8, GR88) or one layer of two planes.
    std::array<gpu::DmaBufImage, 2> out;
    for (int i = 0; i < 2; ++i) {
        const bool layered = desc.nb_layers == 2;
        if ((!layered && (desc.nb_layers != 1 || desc.layers[0].nb_planes != 2)) ||
            (layered && desc.layers[i].nb_planes != 1))
            return make_error(ErrorCode::Unsupported, Category::Encode,
                              "unexpected VA-API surface layout");
        const AVDRMPlaneDescriptor& plane =
            layered ? desc.layers[i].planes[0] : desc.layers[0].planes[i];
        if (plane.object_index < 0 || plane.object_index >= desc.nb_objects)
            return make_error(ErrorCode::Unsupported, Category::Encode,
                              "unexpected VA-API surface object");
        const AVDRMObjectDescriptor& object = desc.objects[plane.object_index];
        auto image = gpu::DmaBufImage::import(
            device,
            gpu::DmaBufPlane{.fd = object.fd,
                             .offset = static_cast<std::uint64_t>(plane.offset),
                             .pitch = static_cast<std::uint64_t>(plane.pitch),
                             .modifier = object.format_modifier,
                             .format = i == 0 ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8G8_UNORM,
                             .width = static_cast<std::uint32_t>(i == 0 ? width : width / 2),
                             .height = static_cast<std::uint32_t>(i == 0 ? height : height / 2),
                             .writable = true});
        if (!image)
            return std::unexpected(image.error());
        out[static_cast<std::size_t>(i)] = std::move(*image);
    }
    return out;
}

} // namespace oma::media
