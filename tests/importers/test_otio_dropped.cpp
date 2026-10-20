// OTIO data a track, stack or clip carries that the importer does not apply: reported, and a
// disabled clip left as a gap, never played as if enabled.
#include "oma/importers/otio.hpp"

#include <algorithm>
#include <string>
#include <variant>

#include "oma_test.hpp"

namespace ir = oma::ir;

namespace {

constexpr auto kRange = R"({"OTIO_SCHEMA":"TimeRange.1",)"
                        R"("start_time":{"OTIO_SCHEMA":"RationalTime.1","rate":24,"value":0},)"
                        R"("duration":{"OTIO_SCHEMA":"RationalTime.1","rate":24,"value":12}})";

// One clip on one track; `clip`, `track` and `stack` are extra members of each.
oma::Result<oma::importers::Imported> one_clip(const std::string& clip, const std::string& track,
                                               const std::string& stack) {
    const std::string media = R"("media_reference":{"OTIO_SCHEMA":"ExternalReference.1",)"
                              R"("target_url":"/m/a.mp4"})";
    return oma::importers::import_otio(
        R"({"OTIO_SCHEMA":"Timeline.1","name":"t","tracks":{"OTIO_SCHEMA":"Stack.1",)" + stack +
        R"("children":[{"OTIO_SCHEMA":"Track.1","name":"V1","kind":"Video",)" + track +
        R"("children":[{"OTIO_SCHEMA":"Clip.1","name":"c",)" + clip + media +
        R"(,"source_range":)" + kRange + "}]}]}}");
}

bool says(const oma::Result<oma::importers::Imported>& r, const std::string& reason) {
    return r && std::ranges::any_of(r->report.issues,
                                    [&](const ir::ImportIssue& i) { return i.reason == reason; });
}

bool disabled_clip_is_a_gap() {
    const auto r = one_clip(R"("enabled":false,)", "", "");
    const auto* gap = r ? std::get_if<ir::Gap>(&r->timeline.tracks[0].items[0]) : nullptr;
    return gap && gap->duration.seconds_approx() == 0.5 && says(r, "clip disabled in the source");
}

bool reports_track_and_stack() {
    const auto r = one_clip("",
                            R"("enabled":false,"markers":[{"OTIO_SCHEMA":"Marker.2"}],)"
                            R"("source_range":)" +
                                std::string(kRange) + ",",
                            R"("markers":[{"OTIO_SCHEMA":"Marker.2"}],)");
    return r && r->timeline.tracks[0].off && says(r, "track markers not imported") &&
           says(r, "track source_range not imported") && says(r, "stack markers not imported") &&
           std::holds_alternative<ir::Clip>(r->timeline.tracks[0].items[0]);
}

} // namespace

void run_otio_dropped_tests() {
    describe("OTIO data not applied", {
        it("leaves a disabled clip as a gap of its length",
           { expect(disabled_clip_is_a_gap()).toBeTruthy(); });
        it("reports a track's and the stack's markers, trim and disabled flag",
           { expect(reports_track_and_stack()).toBeTruthy(); });
    });
}
