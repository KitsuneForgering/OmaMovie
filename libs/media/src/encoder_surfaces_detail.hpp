#pragma once

// EncoderSurfaces' state (encoder_surfaces.cpp), made by VideoWriter::surfaces.

#include "ffmpeg.hpp"
#include "oma/gpu/device.hpp"
#include "oma/gpu/resources.hpp"
#include "oma/media/encoder_surfaces.hpp"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_drm.h>
}

#include <array>
#include <cstdint>
#include <map>

namespace oma::media {

struct EncoderSurfaces::Impl {
    const gpu::Device* device = nullptr;
    ff::BufferPtr frames; // the encoder's VA-API frames context (its own reference)
    std::map<std::uintptr_t, std::array<gpu::DmaBufImage, 2>> imported; // by VA surface ID
};

// An NV12 surface's luma (R8) and CbCr (RG8) planes as writable images on `device`.
[[nodiscard]] Result<std::array<gpu::DmaBufImage, 2>>
import_nv12(const gpu::Device& device, const AVDRMFrameDescriptor& desc, int width, int height);

} // namespace oma::media
