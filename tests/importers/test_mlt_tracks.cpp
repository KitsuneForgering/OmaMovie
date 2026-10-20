// MLT track flags: hide="both" turns a track off (neither picture nor sound).
#include "oma/importers/mlt.hpp"

#include <string>

#include "oma_test.hpp"

namespace {

bool off(const char* hide) {
    const auto r = oma::importers::import_mlt(
        std::string(R"(<mlt><profile frame_rate_num="25" frame_rate_den="1"/>)"
                    R"(<producer id="p"><property name="resource">/m/a.mp4</property></producer>)"
                    R"(<playlist id="l"><entry producer="p" in="0" out="9"/></playlist>)"
                    R"(<tractor id="t"><track producer="l" hide=")") +
        hide + R"("/></tractor></mlt>)");
    return r && r->timeline.tracks.size() == 1 && r->timeline.tracks[0].off;
}

} // namespace

void run_mlt_track_tests() {
    describe("MLT track flags", {
        it("imports a track hidden from both picture and sound as off", {
            expect(off("both")).toBeTruthy();
            expect(off("")).toBeFalsy();
            expect(off("video")).toBeFalsy(); // an audio track
        });
    });
}
