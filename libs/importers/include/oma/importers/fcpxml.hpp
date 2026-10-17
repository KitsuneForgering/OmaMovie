#pragma once

// Final Cut Pro XML (`.fcpxml`, and the `Info.fcpxml` of an `.fcpxmld` bundle) to ProjectIR
// (ADR-0008), read-only. Read: `format` frame durations, `asset` files (the `src` attribute up to
// 1.8, `media-rep` from 1.9) with their start and duration, the first sequence's spine
// (`asset-clip`, `clip`, `gap`, `transition`) and connected clips (`lane`) as further tracks.
// Titles, generators, effects and other projects are kept as opaque objects and reported.
// Docs/formats/fcpxml.md has the format notes; Docs/interop.md the support level.

#include "oma/base/error.hpp"
#include "oma/importers/otio.hpp"

#include <cstddef>
#include <string_view>

namespace oma::importers {

inline constexpr std::size_t kMaxFcpxmlBytes = std::size_t{64} << 20;

// Times are FCPXML's rational seconds on the sequence's frame duration (1001/30000 s for 29.97).
[[nodiscard]] Result<Imported> import_fcpxml(std::string_view text);

} // namespace oma::importers
