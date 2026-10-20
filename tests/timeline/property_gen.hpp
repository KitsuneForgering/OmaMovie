#pragma once

// Seeded steps for the edit properties. The generator reads the current timeline so most steps
// meet their command's preconditions, and some deliberately miss them (boundaries, wrong media
// or track, empty positions) to exercise rejection. Its own splitmix64 and modulo keep a seed's
// steps identical across standard libraries, unlike <random> distributions.

#include "oma/timeline/model.hpp"
#include "property_ops.hpp"

#include <cstdint>

namespace property {

// Bumped whenever a seed starts producing different steps, so reports name the generator.
inline constexpr int kGeneratorVersion = 10;

class Rng {
public:
    explicit Rng(std::uint64_t seed) noexcept;
    [[nodiscard]] std::uint64_t next() noexcept;
    // Uniform enough for test generation in [lo, hi].
    [[nodiscard]] std::int64_t between(std::int64_t lo, std::int64_t hi) noexcept;

private:
    std::uint64_t state_;
};

// Lengths in ticks the values are drawn against.
struct Extent {
    std::int64_t clip = 0;     // the target clip, or a default when the position is empty
    std::int64_t timeline = 0; // end of the last clip
    std::int64_t tracks = 0;
};

void choose_values(Rng& rng, Op& op, const Extent& extent);
// Points steps whose preconditions are structural (Roll, Connect) at a clip that meets them.
void aim(Rng& rng, const oma::timeline::Timeline& tl, Op& op);
[[nodiscard]] Op generate(Rng& rng, const oma::timeline::Timeline& tl);

} // namespace property
