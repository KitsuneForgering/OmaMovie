#include "property_gen.hpp"

namespace property {

Rng::Rng(std::uint64_t seed) noexcept : state_(seed) {}

std::uint64_t Rng::next() noexcept {
    std::uint64_t z = (state_ += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31U);
}

std::int64_t Rng::between(std::int64_t lo, std::int64_t hi) noexcept {
    const auto span = static_cast<std::uint64_t>(hi - lo) + 1U;
    return lo + static_cast<std::int64_t>(next() % span);
}

} // namespace property
