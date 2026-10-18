#include "oma/media/video_decoder.hpp"

#include "media_test.hpp"

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "oma_test.hpp"

using oma::media::ColorInfo;
using oma::media::ColorRange;

namespace {

// The first frame of the full-range BT.709 patches (320x180) as RGBA, read with its tags or as
// `read_as`, at `width` x `height` (0: the frame's size).
std::vector<std::uint8_t> patches(std::optional<ColorInfo> read_as, int width = 0, int height = 0) {
    oma::media::VideoDecoderOptions o;
    o.paths = {oma::media::DecodePath::Software};
    auto d = oma::media::VideoDecoder::open(fixture("color_patches_bt709_pc.mkv"), o);
    auto f = d ? (*d)->next() : decltype((*d)->next()){};
    if (!f || !*f)
        return {};
    const auto& frame = **f;
    const int w = width > 0 ? width : frame.width();
    const int h = height > 0 ? height : frame.height();
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(w) * h * 4);
    return frame.copy_rgba_scaled(rgba, w * 4, w, h, read_as) ? rgba : std::vector<std::uint8_t>{};
}

ColorInfo as(std::uint8_t matrix, ColorRange range) {
    ColorInfo c;
    c.matrix = matrix;
    c.range = range;
    return c;
}

// Each pixel of the half-size copy is the mean of its 2x2 block in the full copy, within a few
// levels (the area filter averages after converting; subsampled chroma blurs edges slightly).
bool halves_like_a_box_average() {
    const auto full = patches(std::nullopt);
    const auto half = patches(std::nullopt, 160, 90);
    if (full.size() != 320U * 180 * 4 || half.size() != 160U * 90 * 4)
        return false;
    long worst_mean = 0;
    long total = 0;
    for (int y = 0; y < 90; ++y)
        for (int x = 0; x < 160; ++x)
            for (int c = 0; c < 3; ++c) {
                const auto at = [&](int fx, int fy) {
                    return static_cast<long>(full[((fy * 320 + fx) * 4) + c]);
                };
                const long mean = (at(2 * x, 2 * y) + at((2 * x) + 1, 2 * y) +
                                   at(2 * x, (2 * y) + 1) + at((2 * x) + 1, (2 * y) + 1) + 2) /
                                  4;
                const long d = std::abs(mean - half[((y * 160 + x) * 4) + c]);
                total += d;
                worst_mean = std::max(worst_mean, d);
            }
    return total < 160L * 90 * 3 * 2 && worst_mean < 64;
}

} // namespace

void run_copy_rgba_tests() {
    describe("RGBA preview copy", {
        it("reads YUV with a given matrix and range instead of the frame's tags", {
            if (!have_fixture("color_patches_bt709_pc.mkv"))
                return;
            const auto tagged = patches(std::nullopt);
            expect(tagged.empty()).toBeFalsy();
            // The same reading spelled out changes nothing; another range or matrix does.
            expect(patches(as(1, ColorRange::Full)) == tagged).toBeTruthy();
            expect(patches(as(1, ColorRange::Limited)) == tagged).toBeFalsy();
            expect(patches(as(6, ColorRange::Full)) == tagged).toBeFalsy();
        });
        it("scales while converting, like a box average of the full copy", {
            if (!have_fixture("color_patches_bt709_pc.mkv"))
                return;
            expect(halves_like_a_box_average()).toBeTruthy();
        });
    });
}
