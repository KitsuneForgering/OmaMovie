#pragma once

#include "viewer_frame.hpp"

#include <optional>

// Return the first LUT needed by a visible video clip but absent from the export snapshot.
[[nodiscard]] std::optional<oma::timeline::LutId>
missing_export_lut(const oma::timeline::Timeline& timeline, const LutTables& loaded);
