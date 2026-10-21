// MLT track state: hide="both" turns a track off; inert track filters are not reported.
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

// Kdenlive's default track filters, disabled volume and panner and a level meter, change
// nothing and say nothing; an enabled volume is reported.
bool reports_only_active_filters() {
    const auto doc = [](const char* disable) {
        return oma::importers::import_mlt(
            std::string(
                R"(<mlt><profile frame_rate_num="25" frame_rate_den="1"/>)"
                R"(<producer id="p"><property name="resource">/m/a.mp4</property></producer>)"
                R"(<playlist id="l"><entry producer="p" in="0" out="9"/></playlist>)"
                R"(<tractor id="k"><track producer="l"/><filter id="v">)"
                R"(<property name="mlt_service">volume</property><property name="disable">)") +
            disable +
            R"(</property></filter><filter id="m"><property name="mlt_service">audiolevel</property>)"
            R"(</filter></tractor><tractor id="t"><track producer="k"/></tractor></mlt>)");
    };
    const auto quiet = doc("1");
    const auto loud = doc("0");
    return quiet && loud && quiet->report.issues.empty() && loud->report.issues.size() == 1 &&
           loud->report.issues[0].object == "v" &&
           quiet->timeline.unsupported.size() == 2; // both still kept for a round trip
}

} // namespace

void run_mlt_track_tests() {
    describe("MLT track flags", {
        it("imports a track hidden from both picture and sound as off", {
            expect(off("both")).toBeTruthy();
            expect(off("")).toBeFalsy();
            expect(off("video")).toBeFalsy(); // an audio track
        });
        it("reports only filters that change the output",
           { expect(reports_only_active_filters()).toBeTruthy(); });
    });
}
