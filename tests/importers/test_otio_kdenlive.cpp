// What Kdenlive's own OTIO exports contain (its tests/dataset, read 2026-10-20): every track
// carries its whole extent as source_range, stills an available_range at rate 0, and clips a
// range at the timeline's rate against media ranges at the media's. None of it is a refusal.
#include "otio_test.hpp"

#include <algorithm>

#include "oma_test.hpp"

namespace ir = oma::ir;

namespace {

// basic.otio with one text replaced.
oma::Result<oma::importers::Imported> with(std::string_view from, std::string_view to) {
    std::string text = read_fixture("basic.otio");
    text.replace(text.find(from), from.size(), to);
    return oma::importers::import_otio(text);
}

bool says(const oma::Result<oma::importers::Imported>& r, std::string_view reason) {
    return r && std::ranges::any_of(r->report.issues,
                                    [&](const ir::ImportIssue& i) { return i.reason == reason; });
}

} // namespace

void run_otio_kdenlive_tests() {
    describe("OTIO as Kdenlive exports it", {
        // 240 frames of media: a clip from frame 24 for 480 frames runs past its media's range.
        it("drops an available range a clip runs outside, and says so", {
            const auto r = with("\"value\": 48.0", "\"value\": 480.0");
            expect(r && !r->timeline.media[0].available &&
                   says(r, "clip runs outside its media's available_range"))
                .toBeTruthy();
        });
        it("ignores an available range at rate 0, and says so", {
            const auto r = with("\"rate\": 23.976023976023978, \"value\": 0.0",
                                "\"rate\": 0.0, \"value\": 0.0");
            expect(r && says(r, "media available_range unreadable")).toBeTruthy();
        });
    });
}
