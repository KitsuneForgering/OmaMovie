// PreviewAdapt: the adaptive preview halves resolution after sustained drops, not after the drops
// every playback start has.
#include "preview_adapt.hpp"

#include "oma_test.hpp"

namespace {
// Shows `frames` frames, `drops` of them overtaken (counted on the first frames); true if any
// window asked for half resolution.
bool halves(PreviewAdapt& adapt, std::int64_t& dropped, int frames, int drops) {
    bool half = false;
    for (int i = 0; i < frames; ++i) {
        if (i < drops)
            ++dropped;
        half = adapt.shown(dropped) || half;
    }
    return half;
}

bool ignores_start_drops() {
    PreviewAdapt adapt;
    std::int64_t dropped = 0;
    adapt.restart(dropped);
    // Five drops in the first 12 frames as playback starts (a 4K start measured 3-9), then clean
    // frames well past a window. The count is literal, not kSettleFrames: it is the measurement.
    return !halves(adapt, dropped, 12, 5) &&
           !halves(adapt, dropped, 2 * PreviewAdapt::kWindowFrames, 0);
}

bool halves_after_sustained_drops() {
    PreviewAdapt adapt;
    std::int64_t dropped = 0;
    adapt.restart(dropped);
    const bool settled = !halves(adapt, dropped, PreviewAdapt::kSettleFrames, 0);
    // 2 of 120 is under 2%; 3 of 120 is over it.
    const bool two = halves(adapt, dropped, PreviewAdapt::kWindowFrames, 2);
    const bool three = halves(adapt, dropped, PreviewAdapt::kWindowFrames, 3);
    return settled && !two && three && adapt.window_dropped() == 3 &&
           adapt.window_shown() == PreviewAdapt::kWindowFrames;
}
} // namespace

void run_preview_adapt_tests() {
    describe("Adaptive preview rule", {
        it("ignores the drops of a playback start",
           { expect(ignores_start_drops()).toBeTruthy(); });
        it("halves after more than 2% of a later window drops",
           { expect(halves_after_sustained_drops()).toBeTruthy(); });
    });
}
