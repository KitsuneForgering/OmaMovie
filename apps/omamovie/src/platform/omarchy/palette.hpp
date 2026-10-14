#pragma once

#include <QByteArray>
#include <QColor>
#include <QVariantMap>

namespace omarchy {
// Palette keys used by the UI, with dark neutral fallbacks.
[[nodiscard]] QVariantMap fallback_palette();

// Apply valid known key/value lines from omarchy-theme-color --all over a base palette.
// accepted reports whether at least one value was accepted.
[[nodiscard]] QVariantMap merge_palette(const QVariantMap& base, const QByteArray& output,
                                        bool* accepted);

// WCAG 2.x contrast ratio of two colours, 1 to 21.
[[nodiscard]] double contrast_ratio(const QColor& a, const QColor& b);

// The palette with its secondary text colour (dark_foreground: durations, labels, timecodes)
// raised to 4.5:1 against the background (WCAG 1.4.3 AA) by mixing it toward the theme's
// foreground just enough; a theme that already meets it is unchanged. Most bundled Omarchy
// themes set it between 1.7:1 and 4.4:1.
[[nodiscard]] QVariantMap readable_palette(QVariantMap palette);
} // namespace omarchy
