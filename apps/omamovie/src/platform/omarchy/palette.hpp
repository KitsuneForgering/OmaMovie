#pragma once

#include <QByteArray>
#include <QVariantMap>

namespace omarchy {
// Palette keys used by the UI, with dark neutral fallbacks.
[[nodiscard]] QVariantMap fallback_palette();

// Apply valid known key/value lines from omarchy-theme-color --all over a base palette.
// accepted reports whether at least one value was accepted.
[[nodiscard]] QVariantMap merge_palette(const QVariantMap& base, const QByteArray& output,
                                        bool* accepted);
} // namespace omarchy
