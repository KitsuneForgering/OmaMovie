#pragma once

// Video scopes (M9) from a display-encoded picture: what the viewer shows, measured, never tinted
// by the theme (ui-design §10). Computed on a small render of the paused frame, off the UI thread.

#include "oma/compositor/compositor.hpp"

#include <QImage>

#include <functional>

// A waveform of any per-pixel level in [0, 255]: one column per picture column, level 0 at the
// bottom, brighter where more pixels share a level. `picture.width()` × 256 grayscale.
[[nodiscard]] QImage level_waveform(const QImage& picture,
                                    const std::function<int(QRgb)>& level_of);
// An RGB parade: the red, green and blue waveforms side by side, each a third of the width.
[[nodiscard]] QImage rgb_parade(const QImage& picture);

// A luma waveform: one column per picture column, Y' (BT.709 weights on the display-encoded
// values) from 0 at the bottom to 255 at the top, brighter where more pixels share a level.
// `picture` is any 8-bit RGB(A) image; the result is `picture.width()` × 256 grayscale.
[[nodiscard]] QImage luma_waveform(const QImage& picture);

// A vectorscope: each pixel's BT.709 colour difference (Cb right, Cr up) on a 256 × 256 plane
// centred on neutral, from the display-encoded values; the edge is full saturation (±0.5).
[[nodiscard]] QImage vectorscope(const QImage& picture);

// Linear premultiplied RGBA (the compositor's working space, ADR-0006) as an sRGB-encoded 8-bit
// image: the display transform, for previews and scopes only.
[[nodiscard]] QImage to_srgb_image(const oma::compositor::RgbaImage& image);
