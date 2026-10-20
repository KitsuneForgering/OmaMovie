#include "fixture.hpp"
#include "property_invariants.hpp"
#include "property_lookup.hpp"

#include <algorithm>
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
constexpr auto kSepia = "oma.look.sepia";

// Sepia's amount (declared in [0, 1]) set in place, or sepia added; a < 0 names a parameter
// sepia does not declare.
void set_sepia(VideoProperties& v, const Op& op) {
    const EffectParam p{.name = op.a < 0 ? "nonsense" : "amount",
                        .value = static_cast<double>(op.c) / 4};
    auto it = std::ranges::find(v.effects, std::string(kSepia), &Effect::definition);
    if (it == v.effects.end())
        v.effects.push_back(Effect{.definition = kSepia, .enabled = true, .params = {p}});
    else
        it->params = {p};
}
} // namespace

std::unique_ptr<Command> video(const Op& op, const Clip* clip) {
    VideoProperties v = clip ? clip->video : VideoProperties{};
    if (op.b == 0) {
        v.opacity_keys.push_back(ScalarKey{.at = f(op.a), .value = static_cast<double>(op.c) / 4});
    } else if (op.b == 1) {
        const auto index = static_cast<std::size_t>(std::abs(op.a)) % kEffects.size();
        v.effects.push_back(
            Effect{.definition = kEffects.at(index), .enabled = true, .params = {}});
    } else if (op.b == 3) {
        Transform t = v.transform;
        t.scale_x = t.scale_y = static_cast<double>(op.c) / 2; // 0 is refused
        v.transform_keys.push_back(TransformKey{.at = f(op.a), .value = t});
    } else if (op.b == 4) {
        set_sepia(v, op);
    } else {
        v.opacity_keys.clear();
        v.transform_keys.clear();
        v.effects.clear();
    }
    return edit::set_video(clip ? clip->id : ClipId{}, std::move(v));
}

std::string broken_video(const Clip& c) {
    const VideoProperties& v = c.video;
    std::set<std::string> definitions;
    for (const Effect& e : v.effects) {
        if (!definitions.insert(e.definition).second)
            return std::format("clip {}: effect {} twice", c.id.value(), e.definition);
        std::set<std::string> names;
        for (const EffectParam& p : e.params)
            if (!names.insert(p.name).second || !std::isfinite(p.value) ||
                (e.definition == kSepia && (p.name != "amount" || p.value < 0 || p.value > 1)))
                return std::format("clip {}: parameter {} of {}", c.id.value(), p.name,
                                   e.definition);
    }
    if (v.effects.size() > kMaxEffects || v.opacity_keys.size() > kMaxKeys ||
        v.transform_keys.size() > kMaxKeys)
        return std::format("clip {}: too many effects or keys", c.id.value());
    for (std::size_t i = 0; i < v.opacity_keys.size(); ++i) {
        const ScalarKey& k = v.opacity_keys[i];
        if (!std::isfinite(k.value) || k.value < 0 || k.value > 1 ||
            (i > 0 && !(v.opacity_keys[i - 1].at < k.at)))
            return std::format("clip {}: opacity key {} out of range or order", c.id.value(), i);
    }
    for (std::size_t i = 0; i < v.transform_keys.size(); ++i) {
        const Transform& t = v.transform_keys[i].value;
        if (!std::isfinite(t.scale_x) || !std::isfinite(t.scale_y) || t.scale_x == 0 ||
            t.scale_y == 0 || (i > 0 && !(v.transform_keys[i - 1].at < v.transform_keys[i].at)))
            return std::format("clip {}: transform key {} invalid or out of order", c.id.value(),
                               i);
    }
    return {};
}

} // namespace property
