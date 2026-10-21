// Kdenlive 23.04+ project structure: the main tractor holds a sequence tractor, whose tracks are
// per-track tractors of two playlists (A/B), as in Kdenlive's tests/dataset (clip-ids.kdenlive).
#include "oma/importers/mlt.hpp"

#include <algorithm>
#include <string>

#include "oma_test.hpp"

namespace ir = oma::ir;

namespace {

std::string transition(const char* id, const char* service, const char* extra) {
    return std::string(R"(<transition id=")") + id + R"("><property name="mlt_service">)" +
           service + "</property>" + extra + "</transition>";
}

// A luma inside the A/B pair is a same-track mix; Kdenlive's automatic track compositing
// (internal_added=237) says nothing; another transition in the sequence is between tracks.
bool classifies_kdenlive_transitions() {
    const auto r = oma::importers::import_mlt(
        R"(<mlt><profile frame_rate_num="25" frame_rate_den="1"/>)"
        R"(<producer id="p"><property name="resource">/m/a.mp4</property></producer>)"
        R"(<playlist id="a"><entry producer="p" in="0" out="9"/></playlist><playlist id="b"/>)"
        R"(<tractor id="track"><track producer="a"/><track producer="b"/>)" +
        transition("mixA", "luma", "") +
        R"(</tractor><tractor id="seq"><track producer="track"/>)" +
        transition("auto", "qtblend", R"(<property name="internal_added">237</property>)") +
        transition("wipe", "composite", "") +
        R"(</tractor><tractor id="main"><track producer="seq"/></tractor></mlt>)");
    const auto said = [&](const std::string& object, const std::string& reason) {
        return std::ranges::any_of(r->report.issues, [&](const ir::ImportIssue& i) {
            return i.object == object && i.reason == reason;
        });
    };
    return r && r->report.issues.size() == 2 &&
           said("mixA", "luma within a track (a same-track mix) not imported") &&
           said("wipe", "composite between tracks not imported");
}

} // namespace

void run_mlt_kdenlive_tests() {
    describe("MLT as Kdenlive writes it", {
        it("tells same-track mixes from track compositing and transitions between tracks",
           { expect(classifies_kdenlive_transitions()).toBeTruthy(); });
    });
}
