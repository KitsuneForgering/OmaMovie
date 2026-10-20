#include "fixture.hpp"
#include "property_invariants.hpp"
#include "property_lookup.hpp"

#include <array>
#include <cmath>
#include <format>
#include <set>
#include <string>

namespace property {

using namespace timeline_test;

namespace {
// Two looks, the detail effect, an unknown well-formed definition (kept, as a missing pack would
// be) and an ill-formed name the model refuses.
constexpr std::array<const char*, 5> kEffects{"oma.look.sepia", "oma.look.cool", "oma.detail",
                                              "test.unknown", "Bad Name!"};
} // namespace

std::unique_ptr<Command> video(const Op& op, const Clip* clip) {
    VideoProperties v = clip ? clip->video : VideoProperties{};
    if (op.b == 0) {
        v.opacity_keys.push_back(ScalarKey{.at = f(op.a), .value = static_cast<double>(op.c) / 4});
    } else if (op.b == 1) {
        const auto index = static_cast<std::size_t>(std::abs(op.a)) % kEffects.size();
        v.effects.push_back(
            Effect{.definition = kEffects.at(index), .enabled = true, .params = {}});
    } else {
        v.opacity_keys.clear();
        v.effects.clear();
    }
    return edit::set_video(clip ? clip->id : ClipId{}, std::move(v));
}

std::string broken_video(const Clip& c) {
    const VideoProperties& v = c.video;
    std::set<std::string> definitions;
    for (const Effect& e : v.effects)
        if (!definitions.insert(e.definition).second)
            return std::format("clip {}: effect {} twice", c.id.value(), e.definition);
    if (v.effects.size() > kMaxEffects || v.opacity_keys.size() > kMaxKeys)
        return std::format("clip {}: too many effects or keys", c.id.value());
    for (std::size_t i = 0; i < v.opacity_keys.size(); ++i) {
        const ScalarKey& k = v.opacity_keys[i];
        if (!std::isfinite(k.value) || k.value < 0 || k.value > 1 ||
            (i > 0 && !(v.opacity_keys[i - 1].at < k.at)))
            return std::format("clip {}: opacity key {} out of range or order", c.id.value(), i);
    }
    return {};
}

} // namespace property
