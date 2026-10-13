#pragma once

// Video scopes (M9) from a display-encoded picture: what the viewer shows, measured, never tinted
// by the theme (ui-design §10). Computed on a small render of the paused frame, off the UI thread.

#include "oma/compositor/compositor.hpp"

#include <QImage>

// A luma waveform: one column per picture column, Y' (BT.709 weights on the display-encoded
// values) from 0 at the bottom to 255 at the top, brighter where more pixels share a level.
// `picture` is any 8-bit RGB(A) image; the result is `picture.width()` × 256 grayscale.
[[nodiscard]] QImage luma_waveform(const QImage& picture);

// Linear premultiplied RGBA (the compositor's working space, ADR-0006) as an sRGB-encoded 8-bit
// image: the display transform, for previews and scopes only.
[[nodiscard]] QImage to_srgb_image(const oma::compositor::RgbaImage& image);
