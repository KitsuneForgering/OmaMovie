#include "property_fraction.hpp"
#include "property_invariants.hpp"

#include <algorithm>
#include <format>

namespace property {

using namespace oma::timeline;

namespace {

// Media displacement in seconds per sequence tick at a speed.
Fraction step(const Timeline& tl, oma::Rational speed) {
    return of(tl.timebase()) * of(speed);
}

} // namespace

std::string broken_extent(const Timeline& tl, const Clip& c) {
    const MediaInfo* media = tl.find_media(c.media);
    if (c.title || !media || media->still)
        return {};
    const Fraction ticks{c.duration.value(), 1};
    Fraction low{};
    Fraction high{};
    if (c.time_map.is_constant()) {
        high = ticks * step(tl, c.time_map.speed());
    } else {
        // Displacement is monotonic within a segment (ramps never change direction), so the
        // extremes lie on segment boundaries.
        Fraction at{};
        Fraction covered{};
        for (const TimeSegment& s : c.time_map.segments()) {
            const Fraction length{s.length, 1};
            if (s.kind == TimeSegment::Kind::Linear)
                at = at + (length * step(tl, s.from));
            else if (s.kind == TimeSegment::Kind::Ramp)
                at = at + (length * step(tl, s.from) * Fraction{1, 2}) +
                     (length * step(tl, s.to) * Fraction{1, 2});
            covered = covered + length;
            low = std::min(low, at, [](Fraction x, Fraction y) { return x < y; });
            high = std::max(high, at, [](Fraction x, Fraction y) { return x < y; });
        }
        if (covered < ticks || ticks < covered)
            return std::format("clip {}: its map covers {} ticks of {}", c.id.value(),
                               static_cast<std::int64_t>(covered.n), c.duration.value());
    }
    const Fraction in = seconds(c.source_in);
    const Fraction first = seconds(media->start);
    const Fraction last = first + seconds(media->duration);
    if (in + low < first || last < in + high)
        return std::format("clip {}: shows media outside the source range", c.id.value());
    return {};
}

} // namespace property
