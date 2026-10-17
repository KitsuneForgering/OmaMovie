// M10 effects contract, decoded exports: every shipped look, exported through the Vulkan
// compositor and the GPU-made NV12, decodes (ffmpeg) as close to the CPU reference's export as
// the clip without a look does (two compositors and two lossy encodes: ~1 level), and visibly
// differs from that clip.
#include "export_look.hpp"

#include <cstdio>

#include "oma_test.hpp"

namespace {
using export_look::exported;
using export_look::mean_diff;

bool every_look_exports_like_the_reference() {
    auto device = oma::gpu::Device::create();
    QTemporaryDir dir;
    if (!device || !dir.isValid())
        return device ? false : (std::printf("    (skipped: no Vulkan device)\n"), true);
    const QByteArray plain = exported(dir, "", nullptr);
    const double base = mean_diff(exported(dir, "", device->get()), plain);
    std::printf("    no look: GPU vs CPU %.2f levels\n", base);
    bool ok = !plain.isEmpty();
    for (const auto& d : oma::timeline::effect_definitions()) {
        if (d.stage != oma::timeline::EffectStage::Look)
            continue;
        const double gpu =
            mean_diff(exported(dir, d.id, device->get()), exported(dir, d.id, nullptr));
        const double look = mean_diff(exported(dir, d.id, nullptr), plain);
        std::printf("    %.*s: GPU vs CPU %.2f, look vs none %.2f levels\n",
                    static_cast<int>(d.id.size()), d.id.data(), gpu, look);
        ok = ok && gpu < base + 0.5 && look > 1.0; // the look adds no GPU/CPU disagreement
    }
    return ok;
}

} // namespace

void run_export_look_tests() {
    describe("Exported looks", {
        it("decode like the CPU reference for every shipped look",
           { expect(every_look_exports_like_the_reference()).toBeTruthy(); });
    });
}
