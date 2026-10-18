// The export transfer (ADR-0006): decoding BT.709 video applies the BT.1886 EOTF, so encoding the
// export must invert it. Every 8-bit level of an ungraded picture comes back as itself; the
// BT.709 camera OETF used before darkened mid-tones (level 128 came back as 109).
#include "exporter.hpp"

#include <array>
#include <cmath>

#include "oma_test.hpp"

namespace {

int worst_level_error() {
    std::array<float, 256 * 4> linear{};
    for (int k = 0; k < 256; ++k)
        for (int c = 0; c < 3; ++c)
            linear[(k * 4) + c] = static_cast<float>(std::pow(k / 255.0, 2.4));
    std::array<std::uint8_t, 256 * 3> encoded{};
    encode_bt709(linear.data(), 256, encoded.data());
    int worst = 0;
    for (int k = 0; k < 256; ++k)
        for (int c = 0; c < 3; ++c)
            worst = std::max(worst, std::abs(encoded[(k * 3) + c] - k));
    return worst;
}

} // namespace

void run_export_transfer_tests() {
    describe("Export transfer", {
        it("brings every 8-bit level of an ungraded picture back as itself",
           { expect(worst_level_error()).toBe(0); });
    });
}
