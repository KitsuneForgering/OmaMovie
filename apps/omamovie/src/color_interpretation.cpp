#include "color_interpretation.hpp"

namespace tl = oma::timeline;

void interpret(oma::media::ColorInfo& color, const tl::MediaInfo& media) {
    if (color.matrix == 0)
        return; // an RGB picture (images, planar GBR) has no YUV to read differently
    // H.273 matrix coefficients: 1 BT.709, 6 BT.601 (SMPTE 170M), 9 BT.2020 non-constant.
    switch (media.color_matrix) {
    case tl::MediaColorMatrix::Bt601: color.matrix = 6; break;
    case tl::MediaColorMatrix::Bt709: color.matrix = 1; break;
    case tl::MediaColorMatrix::Bt2020: color.matrix = 9; break;
    case tl::MediaColorMatrix::Auto: break;
    }
    switch (media.color_range) {
    case tl::MediaColorRange::Limited: color.range = oma::media::ColorRange::Limited; break;
    case tl::MediaColorRange::Full: color.range = oma::media::ColorRange::Full; break;
    case tl::MediaColorRange::Auto: break;
    }
}
