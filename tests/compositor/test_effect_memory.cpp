// M10 effects contract, memory per pass: the device memory each shipped effect adds to a warmed
// compositor, read from its live blur/sharpen image allocation sizes. The driver's heap budget
// fluctuates during renders and can decrease between samples, so it cannot enforce this bound.
// Looks run inside the composite pass and should add nothing; detail keeps two RGBA16F images
// per layer, sized by its reduction factor.
#include "effect_memory.hpp"

#include <cstdio>
#include <utility>

#include "oma_test.hpp"

using oma::compositor::Layer;

// Looks add nothing measurable; detail stays within its two full-size RGBA16F images (16 bytes a
// source pixel at most; the 1080p source is the bound at both output sizes).
namespace {

bool effects_stay_within_their_images() {
    const oma::gpu::Device* device = compositor_test_device();
    auto a = device ? decode_first("screenrecording_like.mp4", device) : DecodedFrame{};
    if (!a.frame)
        return true; // no Vulkan device or fixture: nothing to measure
    const double detail_bound = 1920.0 * 1080.0 * 16.0 / (1 << 20) + 1.0;
    bool ok = true;
    for (const auto& [w, h] : {std::pair{1920U, 1080U}, std::pair{3840U, 2160U}}) {
        std::printf("    %ux%u MiB:", w, h);
        for (int kind = 1; kind <= static_cast<int>(oma::compositor::FilterKind::Vignette);
             ++kind) {
            Layer l;
            l.looks = {{.kind = static_cast<oma::compositor::FilterKind>(kind), .amount = 1.0}};
            const double mib = added_mib(*device, a, w, h, l);
            std::printf(" look%d %+.1f", kind, mib);
            ok = ok && mib >= 0 && mib < 1.0;
        }
        for (const double detail : {-0.6, -1.0, 0.6}) {
            Layer l;
            l.sharpness = detail;
            const double mib = added_mib(*device, a, w, h, l);
            std::printf(" detail%+.1f %+.1f", detail, mib);
            ok = ok && mib >= 0 && mib < detail_bound;
        }
        std::printf("\n");
    }
    return ok;
}

} // namespace

void run_effect_memory_tests() {
    describe("Vulkan compositor memory per effect pass", {
        it("adds nothing for looks and at most two source-size images for detail",
           { expect(effects_stay_within_their_images()).toBeTruthy(); });
    });
}
