#pragma once

// CMX 3600 EDL (`.edl`) to ProjectIR (ADR-0008), read-only: the edit decision lists Resolve,
// Premiere, Avid and others export. Read: TITLE and FCM headers, event lines (cut `C` and
// dissolve `D` on V, A, A2…, AA, B), `* FROM CLIP NAME:` and `* SOURCE FILE:` comments. Wipes,
// keys, motion (`M2`) lines and unknown comments are kept as opaque IR objects and reported.
// Docs/interop.md has the support level; Docs/formats/edl.md the format notes.

#include "oma/base/error.hpp"
#include "oma/importers/otio.hpp"

#include <cstddef>
#include <string_view>

namespace oma::importers {

// The largest EDL text accepted, and the most events read from it.
inline constexpr std::size_t kMaxEdlBytes = std::size_t{16} << 20;
inline constexpr std::size_t kMaxEdlEvents = 100000;

// An EDL names no frame rate: `rate` is the timeline's (30000/1001 with a drop-frame `FCM`
// header or `;` timecodes reads them as drop-frame). Times keep that rate (§16.2).
[[nodiscard]] Result<Imported> import_edl(std::string_view text, Rational rate);

} // namespace oma::importers
