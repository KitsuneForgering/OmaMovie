#pragma once

#include <compare>
#include <cstdint>
#include <functional>

// Stable identifiers for timeline objects (CLAUDE.md §10: clips reference media by stable ID,
// never by pointer or path). Zero is never a valid ID.

namespace oma::timeline {

template <typename Tag>
class Id {
public:
    constexpr Id() noexcept = default;
    explicit constexpr Id(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool valid() const noexcept { return value_ != 0; }

    friend constexpr auto operator<=>(Id, Id) noexcept = default;

private:
    std::uint64_t value_ = 0;
};

using MediaId = Id<struct MediaTag>;
using ClipId = Id<struct ClipTag>;
using TrackId = Id<struct TrackTag>;
using MarkerId = Id<struct MarkerTag>;
using CaptionId = Id<struct CaptionTag>;
using LutId = Id<struct LutTag>;

} // namespace oma::timeline

// Specializing std::hash for a program-defined type is allowed ([namespace.std]); the check
// cannot tell it from a modification of std.
template <typename Tag>
struct std::hash<oma::timeline::Id<Tag>> { // NOLINT(bugprone-std-namespace-modification)
    std::size_t operator()(oma::timeline::Id<Tag> id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value());
    }
};
