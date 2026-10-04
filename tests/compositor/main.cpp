#include "compositor_test.hpp"

#include "oma/base/log.hpp"
#include "oma/gpu/device.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "oma_test.hpp"

void run_geometry_tests();
void run_color_tests();
void run_cpu_compositor_tests();
void run_vulkan_compositor_tests();
void run_grade_tests();

namespace {

std::unique_ptr<oma::gpu::Device> g_device;
bool g_attempted = false;

} // namespace

std::filesystem::path fixture(const char* name) {
    const char* dir = std::getenv("OMA_FIXTURES");
    return std::filesystem::path(dir != nullptr ? dir : "tests/fixtures/generated") / name;
}

const oma::gpu::Device* compositor_test_device() {
    if (!g_attempted) {
        g_attempted = true;
        auto d = oma::gpu::Device::create();
        if (d) {
            g_device = std::move(*d);
        } else {
            std::printf("  (no Vulkan device, GPU cases skip: %s)\n", d.error().summary().c_str());
        }
    }
    return g_device.get();
}

void release_compositor_test_device() {
    g_device.reset();
}

oma::compositor::LayerInput DecodedFrame::input() {
    oma::compositor::LayerInput in;
    in.frame = frame ? &*frame : nullptr;
    if (decoder && decoder->stream().video) {
        const auto& v = *decoder->stream().video;
        in.color = v.color;
        in.rotation = v.rotation;
        in.sample_aspect = v.sample_aspect;
    }
    return in;
}

DecodedFrame decode_first(const char* name, const oma::gpu::Device* device) {
    DecodedFrame out;
    oma::media::VideoDecoderOptions o;
    o.device = device;
    if (device == nullptr) {
        o.paths = {oma::media::DecodePath::Software};
    }
    auto d = oma::media::VideoDecoder::open(fixture(name), o);
    if (!d) {
        return out;
    }
    out.decoder = std::move(*d);
    auto f = out.decoder->next();
    if (f && *f) {
        out.frame.emplace(std::move(**f));
    }
    return out;
}

ImageDiff compare(const oma::compositor::RgbaImage& a, const oma::compositor::RgbaImage& b,
                  float tolerance) {
    ImageDiff diff;
    if (a.width != b.width || a.height != b.height || a.pixels.size() != b.pixels.size()) {
        diff.max_abs = INFINITY;
        diff.over_tolerance = a.pixels.size() / 4;
        return diff;
    }
    for (std::size_t i = 0; i < a.pixels.size(); i += 4) {
        bool over = false;
        for (std::size_t c = 0; c < 4; ++c) {
            const float d = std::abs(a.pixels[i + c] - b.pixels[i + c]);
            diff.max_abs = std::max(diff.max_abs, d);
            // Relative to the magnitude, since half floats keep ~3 significant digits.
            over = over || d > tolerance * std::max(1.0F, std::abs(b.pixels[i + c]));
        }
        diff.over_tolerance += over ? 1 : 0;
    }
    return diff;
}

int main(int argc, char* argv[]) {
    oma::set_log_level_all(oma::LogLevel::Error);
    oma::set_log_level(oma::Category::Media, oma::LogLevel::Off);
    cest_init(argc, argv);
    run_geometry_tests();
    run_color_tests();
    run_cpu_compositor_tests();
    run_vulkan_compositor_tests();
    run_grade_tests();
    release_compositor_test_device();
    return cest_result();
}
