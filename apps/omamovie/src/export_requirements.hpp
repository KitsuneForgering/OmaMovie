#pragma once

#include "viewer_frame.hpp"

#include "oma/timeline/effects.hpp"

#include <optional>

// Return the first LUT needed by a visible video clip but absent from the export snapshot.
[[nodiscard]] std::optional<oma::timeline::LutId>
missing_export_lut(const oma::timeline::Timeline& timeline, const LutTables& loaded);

// The first effect this build cannot render that would change the movie: enabled, on a visible
// video track (ADR-0016). A bypassed one, or one on a hidden track, renders as it was made.
[[nodiscard]] std::optional<oma::timeline::UnknownEffect>
unrendered_export_effect(const oma::timeline::Timeline& timeline);
