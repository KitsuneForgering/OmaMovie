#include "otio_test.hpp"

#include <algorithm>
#include <variant>

#include "oma_test.hpp"

namespace ir = oma::ir;

namespace {
bool reported(const ir::ImportReport& r, std::string_view text) {
    return std::ranges::any_of(r.issues, [&](const ir::ImportIssue& i) {
        return i.reason.find(text) != std::string::npos;
    });
}
} // namespace

void run_otio_tests() {
    describe("OTIO import", {
        it("reads tracks, clips, gaps and transitions with exact NTSC times", {
            auto result = oma::importers::import_otio(read_fixture("basic.otio"));
            expect(result.has_value()).toBeTruthy();
            const ir::Timeline& tl = result->timeline;
            expect(result->report.version).toEqual(std::string("Timeline.1"));
            expect(result->report.reached == ir::SupportLevel::Edits).toBeTruthy();
            expect(tl.global_start == ntsc_frames(86400)).toBeTruthy();
            expect(tl.tracks.size()).toBe(2U);
            const auto& v1 = tl.tracks[0].items;
            expect(v1.size()).toBe(4U);
            const auto& a = std::get<ir::Clip>(v1[0]);
            expect(a.source.start() == ntsc_frames(24) && a.source.duration() == ntsc_frames(48))
                .toBeTruthy();
            expect(std::get<ir::Gap>(v1[1]).duration == ntsc_frames(12)).toBeTruthy();
            expect(std::get<ir::Transition>(v1[2]).kind).toEqual(std::string("SMPTE_Dissolve"));
            // 24.5 frames are kept exactly, in thousandths of a frame.
            expect(std::get<ir::Clip>(v1[3]).source.duration() == ntsc_frames(24500, 1000))
                .toBeTruthy();
            expect(tl.tracks[1].kind == ir::TrackKind::Audio).toBeTruthy();
        });

        it("shares one media item per file and keeps what it cannot import", {
            auto result = oma::importers::import_otio(read_fixture("basic.otio"));
            expect(result->timeline.media.size()).toBe(1U);
            expect(std::get<ir::Clip>(result->timeline.tracks[0].items[0]).unsupported.size())
                .toBe(1U);
            expect(result->timeline.tracks[0].unsupported.size()).toBe(1U);
            expect(result->timeline.tracks[0].unsupported[0].schema)
                .toEqual(std::string("Generator.1"));
            expect(reported(result->report, "effects LinearTimeWarp.1")).toBeTruthy();
            expect(reported(result->report, "Generator.1 is not imported")).toBeTruthy();
        });
    });
}
