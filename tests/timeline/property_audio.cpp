#include "fixture.hpp"
#include "property_invariants.hpp"
#include "property_lookup.hpp"

#include <cmath>
#include <format>
#include <string>

namespace property {

using namespace timeline_test;

std::unique_ptr<Command> audio(const Op& op, const Clip* clip) {
    AudioProperties a = clip ? clip->audio : AudioProperties{};
    if (op.b == 0) {
        a.gain_keys.push_back(ScalarKey{.at = f(op.a), .value = static_cast<double>(op.c) * 4});
    } else if (op.b == 1) {
        a.fade_in = f(op.a);
        a.fade_out = f(op.c * 3 - 2);
    } else if (op.b == 2) {
        a.eq.low_db = static_cast<float>(op.a) - 10.0F;
    } else {
        a.gain_keys.clear();
        a.fade_in = a.fade_out = {};
        a.eq = {};
    }
    return edit::set_audio(clip ? clip->id : ClipId{}, a);
}

std::string broken_audio(const Clip& c) {
    const AudioProperties& a = c.audio;
    for (std::size_t i = 0; i < a.gain_keys.size(); ++i) {
        const ScalarKey& k = a.gain_keys[i];
        if (!std::isfinite(k.value) || k.value < 0 || k.value > 16 ||
            (i > 0 && !(a.gain_keys[i - 1].at < k.at)))
            return std::format("clip {}: gain key {} out of range or order", c.id.value(), i);
    }
    if (a.fade_in.value() < 0 || a.fade_out.value() < 0 ||
        a.fade_in.seconds_approx() + a.fade_out.seconds_approx() >
            c.duration.seconds_approx() + 1e-9)
        return std::format("clip {}: fades negative or longer than the clip", c.id.value());
    if (std::abs(a.eq.low_db) > 24 || std::abs(a.eq.mid_db) > 24 || std::abs(a.eq.high_db) > 24)
        return std::format("clip {}: equalizer outside ±24 dB", c.id.value());
    return {};
}

} // namespace property
