#pragma once

#include "oma/base/error.hpp"
#include "oma/base/rational.hpp"
#include "oma/base/time.hpp"
#include "oma/timeline/model.hpp"

#include <cstdint>
#include <string>
#include <vector>

// Internal write access to a Timeline. Only commands (and the Editor's ID allocation) use it,
// which keeps Editor::execute the single entry point for edits.

namespace oma::timeline::detail {

class Mutation {
public:
    explicit Mutation(Timeline& timeline) noexcept : t_(timeline) {}

    [[nodiscard]] std::vector<Track>& tracks() noexcept { return t_.tracks_; }
    [[nodiscard]] std::vector<Marker>& markers() noexcept { return t_.markers_; }
    [[nodiscard]] std::vector<Caption>& captions() noexcept { return t_.captions_; }
    void set_canvas(std::uint32_t width, std::uint32_t height) noexcept {
        t_.canvas_width_ = width;
        t_.canvas_height_ = height;
    }
    [[nodiscard]] std::vector<MediaInfo>& media() noexcept { return t_.media_; }
    [[nodiscard]] std::vector<LutInfo>& luts() noexcept { return t_.luts_; }
    [[nodiscard]] std::uint64_t allocate_id() noexcept { return t_.next_id_++; }

    [[nodiscard]] Track* track(TrackId id) noexcept;

private:
    Timeline& t_;
};

// Exact time arithmetic for source positions, which may use any timebase.

// a * b, reduced; Overflow when the reduced terms do not fit in int64.
[[nodiscard]] Result<Rational> multiply(Rational a, Rational b);
// The largest unit both timebases are integer multiples of (gcd of two positive rationals).
[[nodiscard]] Result<Rational> common_unit(Rational a, Rational b);
// a + b exactly, in their common unit.
[[nodiscard]] Result<RationalTime> add_exact(const RationalTime& a, const RationalTime& b);
// a + b, reduced; Overflow when the reduced terms do not fit in int64.
[[nodiscard]] Result<Rational> add(Rational a, Rational b);
// How far a time-map segment moves through the media after `t` of its ticks, in ticks of
// media at speed 1 (ADR-0013): s*t, 0, or from*t + (to - from)*t^2 / (2*length).
[[nodiscard]] Result<Rational> displacement(const TimeSegment& s, std::int64_t t);
// The segment's speed after `t` of its ticks.
[[nodiscard]] Result<Rational> speed_at(const TimeSegment& s, std::int64_t t);
// Overflow-checked integer addition for sequence ticks.
[[nodiscard]] Result<std::int64_t> add_ticks(std::int64_t a, std::int64_t b);

// Connected clips (ADR-0014), for a primary with a constant time map:
// the sequence tick where `p` shows `source` (rounded up to the grid);
[[nodiscard]] Result<std::int64_t> attach_tick(const Clip& p, const RationalTime& source,
                                               Rational sequence_timebase);
// the media position `p` shows at sequence tick `tick`;
[[nodiscard]] Result<RationalTime> source_at(const Clip& p, std::int64_t tick,
                                             Rational sequence_timebase);
// whether `p` shows `source` at all: source_in <= source < source end.
[[nodiscard]] bool holds(const Clip& p, const RationalTime& source, Rational sequence_timebase);

// The exact media time reached at the end of a clip: source_in + map(duration).
[[nodiscard]] Result<RationalTime> source_end(const Clip& clip, Rational sequence_timebase);

[[nodiscard]] std::unexpected<Error> error(ErrorCode code, std::string message,
                                           std::string context = {});
[[nodiscard]] std::string clip_context(ClipId id);
[[nodiscard]] std::string track_context(TrackId id);

} // namespace oma::timeline::detail
