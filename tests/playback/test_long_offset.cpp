// M10 audio boundaries, long source offsets: a file whose timestamps start three hours in keeps
// its clicks where they belong, whatever the block partition. Matroska stamps packets in whole
// milliseconds, so after a seek the decoder anchors on a rounded time: placement is exact to half
// a tick (24 samples at 48 kHz), the same error the file shows without the offset (2026-10-15).
#include "pulse_clicks.hpp"

#include "oma_test.hpp"

using namespace playback_test;

namespace {

bool clicks_land_within_a_tick() {
    const std::vector<std::int64_t> want{2000, 38'000}; // 1000 + (24000 | 60000 - 23000)
    const auto a = clicks(1024);
    const auto near = [&](const std::vector<std::int64_t>& got) {
        return got.size() == want.size() && std::abs(got[0] - want[0]) <= 24 &&
               got[1] - got[0] == want[1] - want[0];
    };
    return near(a) && clicks(1601) == a;
}

} // namespace

void run_long_offset_tests() {
    describe("TimelineAudio at long source offsets", {
        it("keeps clicks three hours into a file within one container tick", {
            if (have_fixture("pulse_3h.mkv"))
                expect(clicks_land_within_a_tick()).toBeTruthy();
        });
    });
}
