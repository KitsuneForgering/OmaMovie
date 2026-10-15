#pragma once

// MLT XML (`.mlt`, and Kdenlive's `.kdenlive`, which is MLT XML with `kdenlive:` properties) to
// ProjectIR (ADR-0008), read-only. Read: the profile's frame rate, media producers and chains
// (resource, in point, length), playlists of entries and blanks, the main (last) tractor's tracks,
// and melt's mixes (a nested tractor of two producers with a luma/mix transition) as dissolves.
// Filters, cross-track transitions and generators (colour, black) are kept as opaque objects and
// reported. Docs/formats/mlt.md has the format notes; Docs/interop.md the support level.

#include "oma/base/error.hpp"
#include "oma/importers/otio.hpp"

#include <cstddef>
#include <string_view>

namespace oma::importers {

inline constexpr std::size_t kMaxMltBytes = std::size_t{64} << 20;

// The document's text; times keep the profile's rate (25 fps when it has none, reported).
[[nodiscard]] Result<Imported> import_mlt(std::string_view text);

} // namespace oma::importers
