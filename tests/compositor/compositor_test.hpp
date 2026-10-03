#pragma once

// Shared helpers for the libs/compositor tests. Include before oma_test.hpp; test bodies live in
// plain functions (see tests/media/media_test.hpp).

#include "oma/compositor/compositor.hpp"
#include "oma/media/video_decoder.hpp"

#include <filesystem>
#include <memory>
#include <optional>

namespace oma::gpu {
class Device;
}

std::filesystem::path fixture(const char* name);
const oma::gpu::Device* compositor_test_device();
void release_compositor_test_device();

// A decoded first frame and the decoder that owns its surfaces.
struct DecodedFrame {
    std::unique_ptr<oma::media::VideoDecoder> decoder;
    std::optional<oma::media::VideoFrame> frame;
    oma::compositor::LayerInput input();
};

// Software decode unless `device` is given (then the default hardware-first policy).
DecodedFrame decode_first(const char* name, const oma::gpu::Device* device = nullptr);

struct ImageDiff {
    float max_abs = 0.0F;
    std::size_t over_tolerance = 0; // pixels with a channel beyond the tolerance
};
ImageDiff compare(const oma::compositor::RgbaImage& a, const oma::compositor::RgbaImage& b,
                  float tolerance);
