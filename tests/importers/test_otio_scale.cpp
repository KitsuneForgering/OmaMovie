#include "otio_test.hpp"

#include <chrono>
#include <cstdio>
#include <format>

#include "oma_test.hpp"

// Code review 2026-10-14: media were matched by a linear search per clip, O(clips x files).

namespace {

std::string clip(std::size_t file) {
    return std::format(
        R"({{"OTIO_SCHEMA": "Clip.2", "name": "c", "source_range": {{"OTIO_SCHEMA": "TimeRange.1", )"
        R"("start_time": {{"OTIO_SCHEMA": "RationalTime.1", "rate": 24.0, "value": 0.0}}, )"
        R"("duration": {{"OTIO_SCHEMA": "RationalTime.1", "rate": 24.0, "value": 1.0}}}}, )"
        R"("media_references": {{"DEFAULT_MEDIA": {{"OTIO_SCHEMA": "ExternalReference.1", )"
        R"("target_url": "file:///m/{}.mp4"}}}}}})",
        file);
}

// `clips` clips over `files` distinct files, in one video track.
std::string timeline(std::size_t clips, std::size_t files) {
    std::string items;
    for (std::size_t i = 0; i < clips; ++i)
        items += (i == 0 ? "" : ",") + clip(i % files);
    return R"({"OTIO_SCHEMA": "Timeline.1", "name": "t", "tracks": {"OTIO_SCHEMA": "Stack.1", )"
           R"("children": [{"OTIO_SCHEMA": "Track.1", "kind": "Video", "children": [)" +
           items + "]}]}}";
}

} // namespace

void run_otio_scale_tests() {
    describe("OTIO import scale", {
        it("shares one media entry between the clips of a file", {
            const auto imported = oma::importers::import_otio(timeline(6, 2));
            expect(imported.has_value()).toBeTruthy();
            if (imported)
                expect(static_cast<int>(imported->timeline.media.size())).toBe(2);
        });

        it("imports 50,000 clips of distinct files in linear time", {
            const auto json = timeline(50000, 50000);
            const auto start = std::chrono::steady_clock::now();
            const auto imported = oma::importers::import_otio(json);
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - start)
                                .count();
            std::printf("    50,000 clips, 50,000 files: %lld ms\n", static_cast<long long>(ms));
            expect(imported.has_value()).toBeTruthy();
            if (imported)
                expect(static_cast<int>(imported->timeline.media.size())).toBe(50000);
            expect(ms < 10000).toBeTruthy(); // the quadratic search took far longer (see the log)
        });
    });
}
