#include "commands.hpp"

#include "oma_test.hpp"

namespace {
// A canonical dump shape: one member or element per line, two-space indent.
std::vector<std::string_view> sample_dump() {
    return {"{",
            "  \"canvas\": {",
            "    \"width\": 1920",
            "  },",
            "  \"tracks\": [",
            "    {",
            "      \"clips\": [",
            "        {",
            "          \"start\": {",
            "            \"value\": 30,",
            "            \"timebase\": \"1/30\"",
            "          }",
            "        }",
            "      ],",
            "      \"names\": [",
            "        \"a\",",
            "        \"b\"",
            "      ]",
            "    }",
            "  ],",
            "  \"quo\\\"ted\": []",
            "}"};
}
} // namespace

void run_line_paths_tests() {
    describe("line_paths", {
        it("names members, array elements and closing lines of a one-member-per-line dump", {
            const auto dump = sample_dump();
            const auto p = line_paths(dump);
            expect(p.size() == dump.size()).toBeTruthy();
            expect(p[2]).toEqual(std::string("canvas.width"));
            expect(p[3]).toEqual(std::string("canvas"));
            expect(p[9]).toEqual(std::string("tracks[0].clips[0].start.value"));
            expect(p[10]).toEqual(std::string("tracks[0].clips[0].start.timebase"));
            expect(p[16]).toEqual(std::string("tracks[0].names[1]"));
            expect(p[20]).toEqual(std::string("quo\"ted"));
            expect(p[21]).toEqual(std::string());
        });
    });
}
