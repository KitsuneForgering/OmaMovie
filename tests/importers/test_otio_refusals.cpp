#include "otio_test.hpp"

#include "oma_test.hpp"

namespace {
bool refused(const std::string& json) {
    return !oma::importers::import_otio(json).has_value();
}

std::string nested(int levels) {
    return std::string(static_cast<std::size_t>(levels), '[') +
           std::string(static_cast<std::size_t>(levels), ']');
}

// The fixture with one text replaced, to break a single rule.
std::string with(std::string_view from, std::string_view to) {
    std::string text = read_fixture("basic.otio");
    text.replace(text.find(from), from.size(), to);
    return text;
}
} // namespace

void run_otio_refusal_tests() {
    describe("OTIO import refusals", {
        it("refuses text that is not an OTIO timeline", {
            expect(refused("")).toBeTruthy();
            expect(refused("not json")).toBeTruthy();
            expect(refused("[1, 2]")).toBeTruthy();
            expect(refused(R"({"OTIO_SCHEMA": "Clip.2"})")).toBeTruthy();
            expect(refused(R"({"OTIO_SCHEMA": "Timeline.1"})")).toBeTruthy();
            expect(refused(nested(100))).toBeTruthy();
        });

        it("refuses invalid times and clips outside their media", {
            expect(refused(with("\"value\": 24.0", "\"value\": 1e300"))).toBeTruthy();
            expect(refused(with("\"rate\": 23.976023976023978, \"value\": 24.0",
                                "\"rate\": -24, \"value\": 24.0")))
                .toBeTruthy();
            // 240 frames of media: a clip from frame 24 for 480 frames runs past it.
            expect(refused(with("\"value\": 48.0", "\"value\": 480.0"))).toBeTruthy();
        });
    });
}

void run_otio_path_tests() {
    describe("OTIO media paths", {
        it("decodes file URLs and resolves relative targets", {
            using oma::importers::otio_local_path;
            expect(otio_local_path("file:///a%20b/c.mov", "/x") == "/a b/c.mov").toBeTruthy();
            expect(otio_local_path("file:///bad%zz.mov", "/x") == "/bad%zz.mov").toBeTruthy();
            expect(otio_local_path("media/c.mov", "/x") == "/x/media/c.mov").toBeTruthy();
        });
    });
}
