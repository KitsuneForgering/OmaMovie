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
    [[nodiscard]] std::vector<MediaInfo>& media() noexcept { return t_.media_; }
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
// Overflow-checked integer addition for sequence ticks.
[[nodiscard]] Result<std::int64_t> add_ticks(std::int64_t a, std::int64_t b);

// The exact media time reached at the end of a clip: source_in + map(duration).
[[nodiscard]] Result<RationalTime> source_end(const Clip& clip, Rational sequence_timebase);

[[nodiscard]] std::unexpected<Error> error(ErrorCode code, std::string message,
                                           std::string context = {});
[[nodiscard]] std::string clip_context(ClipId id);
[[nodiscard]] std::string track_context(TrackId id);

} // namespace oma::timeline::detail
