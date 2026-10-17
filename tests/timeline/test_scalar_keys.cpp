#include "fixture.hpp"

#include "oma/timeline/evaluate.hpp"

#include <cmath>
#include <vector>

#include "oma_test.hpp"

using namespace timeline_test;

namespace {

ScalarKey key(std::int64_t frame, double value, Interpolation how) {
    return ScalarKey{.at = mf(frame), .value = value, .interpolation = how};
}

// Opacity keys in source time: a camera clip from media frame 0, keys at 0 (1.0) and 10 (0.0).
bool keyed_opacity_evaluates() {
    auto fx = make_fixture();
    edit::ClipSource source = camera(0, 30);
    source.video.opacity_keys = {key(0, 1.0, Interpolation::Linear),
                                 key(10, 0.0, Interpolation::Linear)};
    const ClipId id = fx.editor.new_clip_id();
    if (!fx.editor.execute(edit::append(fx.video, id, source)))
        return false;
    const auto at = [&](std::int64_t frame) {
        auto c = evaluate(fx.editor.timeline(), f(frame));
        return c && !c->video.empty() ? static_cast<double>(c->video.front().video.opacity) : -1.0;
    };
    return std::abs(at(5) - 0.5) < 1e-6 && at(20) == 0.0 && at(0) == 1.0;
}

bool rejects_bad_keys() {
    auto fx = make_fixture();
    edit::ClipSource source = camera(0, 30);
    source.video.opacity_keys = {key(0, 1.5, Interpolation::Linear)}; // above 1
    if (fx.editor.execute(edit::append(fx.video, fx.editor.new_clip_id(), source)))
        return false;
    source.video.opacity_keys = {key(5, 1.0, Interpolation::Linear),
                                 key(5, 0.0, Interpolation::Linear)}; // not increasing
    if (fx.editor.execute(edit::append(fx.video, fx.editor.new_clip_id(), source)))
        return false;
    source.video.opacity_keys.clear();
    source.audio.gain_keys = {key(0, 17.0, Interpolation::Linear)}; // above 16
    return !fx.editor.execute(edit::append(fx.video, fx.editor.new_clip_id(), source));
}

bool interpolations() {
    const std::vector<ScalarKey> keys{key(0, 0.0, Interpolation::Hold),
                                      key(10, 1.0, Interpolation::Ease),
                                      key(20, 0.0, Interpolation::Linear)};
    return scalar_at(keys, mf(5)) == 0.0 && std::abs(scalar_at(keys, mf(15)) - 0.5) < 1e-9 &&
           scalar_at(keys, mf(25)) == 0.0;
}

} // namespace

void run_scalar_key_tests() {
    describe("Opacity and volume keys", {
        it("evaluate opacity in source time", { expect(keyed_opacity_evaluates()).toBeTruthy(); });
        it("refuse values out of range and keys out of order",
           { expect(rejects_bad_keys()).toBeTruthy(); });
        it("hold, interpolate and ease", { expect(interpolations()).toBeTruthy(); });
    });
}
