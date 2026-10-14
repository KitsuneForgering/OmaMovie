#include "oma/media/video_decoder.hpp"

#include "media_test.hpp"

#include <vector>

#include "oma_test.hpp"

using oma::media::ColorInfo;
using oma::media::ColorRange;

namespace {

// The first frame of the full-range BT.709 patches as RGBA, read with its tags or as `read_as`.
std::vector<std::uint8_t> patches(std::optional<ColorInfo> read_as) {
    oma::media::VideoDecoderOptions o;
    o.paths = {oma::media::DecodePath::Software};
    auto d = oma::media::VideoDecoder::open(fixture("color_patches_bt709_pc.mkv"), o);
    auto f = d ? (*d)->next() : decltype((*d)->next()){};
    if (!f || !*f)
        return {};
    const auto& frame = **f;
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(frame.width()) * frame.height() * 4);
    return frame.copy_rgba(rgba, frame.width() * 4, read_as) ? rgba : std::vector<std::uint8_t>{};
}

ColorInfo as(std::uint8_t matrix, ColorRange range) {
    ColorInfo c;
    c.matrix = matrix;
    c.range = range;
    return c;
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
    });
}
