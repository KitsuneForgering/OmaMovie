#include "property_lookup.hpp"

namespace property {

using namespace oma::timeline;

std::unique_ptr<Command> ramp(const Op& op, const Clip* clip) {
    const std::int64_t length = clip ? clip->duration.value() : 1;
    const auto speed = [](std::int64_t halves) {
        return oma::Rational::make(halves, 2).value_or(oma::Rational::literal(0, 1));
    };
    // A ramp the model refuses (zero, reversing or invalid speed) still reaches the edit as a
    // constant map, so the step is rejected by set_time_map rather than here.
    auto map = TimeMap::segmented({TimeSegment{.kind = TimeSegment::Kind::Ramp,
                                               .length = length,
                                               .from = speed(op.a),
                                               .to = speed(op.b)}});
    return edit::set_time_map(clip ? clip->id : ClipId{}, map.value_or(TimeMap{}), op.c != 0);
}

} // namespace property
