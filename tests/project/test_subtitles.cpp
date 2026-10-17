#include "oma/project/subtitles.hpp"

#include <string>
#include <vector>

#include "oma_test.hpp"

using oma::project::Cue;
using oma::project::parse_subtitles;

namespace {

bool same(const std::vector<Cue>& got, const std::vector<Cue>& want) {
    return got == want;
}

Cue cue(std::int64_t start, std::int64_t end, std::string text) {
    return Cue{.start_ms = start, .end_ms = end, .text = std::move(text)};
}

bool reads_srt() {
    const std::string srt = "\xEF\xBB\xBF"
                            "1\r\n00:00:01,000 --> 00:00:02,500\r\nHello\r\nworld\r\n\r\n"
                            "2\r\n00:00:03,000 --> 00:00:04,000 X1:100\r\nSecond\r\n";
    auto cues = parse_subtitles(srt);
    return cues && same(*cues, {cue(1000, 2500, "Hello\nworld"), cue(3000, 4000, "Second")});
}

bool reads_vtt() {
    const std::string vtt =
        "WEBVTT - a title\nKind: captions\n\nNOTE written by hand\nstill the note\n\n"
        "intro\n00:01.000 --> 00:02.000 align:start position:10%\n<v Ana>Oi</v>\n\n"
        "01:00:00.250 --> 01:00:01.000\nOne hour in\n";
    auto cues = parse_subtitles(vtt);
    return cues &&
           same(*cues, {cue(1000, 2000, "<v Ana>Oi</v>"), cue(3600250, 3601000, "One hour in")});
}

// Cues that overlap are cut at the next start; cues are sorted; no blank line between cues works.
bool cuts_overlaps() {
    auto cues = parse_subtitles(
        "00:00:05,000 --> 00:00:09,000\nlate\n00:00:01,000 --> 00:00:06,000\nearly\n");
    return cues && same(*cues, {cue(1000, 5000, "early"), cue(5000, 9000, "late")});
}

bool reports_the_failing_line() {
    auto bad = parse_subtitles(
        "1\n00:00:01,000 --> 00:00:02,000\nok\n\n2\n00:00:0x,000 --> 00:00:03,000\nbad\n");
    auto backwards = parse_subtitles("00:00:03,000 --> 00:00:02,000\nbackwards\n");
    auto long_text =
        parse_subtitles("00:00:01,000 --> 00:00:02,000\n" + std::string(1001, 'x') + "\n");
    auto huge = parse_subtitles(std::string(oma::project::kMaxSubtitleBytes + 1, '\n'));
    return !bad && bad.error().context() == "line 6" && !backwards && !long_text && !huge;
}

// Regression (fuzzer, 2026-10-07): a line of stray carriage returns ended up inside a caption,
// and the written file no longer read back. It is a blank line.
bool carriage_returns_are_blank() {
    auto cues = parse_subtitles("00:00:01,000 --> 00:00:02,000\nHello\n\r\r\nnext block?\n");
    return cues && cues->size() == 1 && (*cues)[0].text == "Hello";
}

bool round_trips() {
    const std::vector<Cue> cues{cue(0, 999, "a"), cue(3723004, 3724000, "two\nlines")};
    auto srt = parse_subtitles(oma::project::write_srt(cues));
    auto vtt = parse_subtitles(oma::project::write_vtt(cues));
    return srt && vtt && same(*srt, cues) && same(*vtt, cues) &&
           oma::project::write_srt(cues).starts_with("1\n00:00:00,000 --> 00:00:00,999\na\n");
}

} // namespace

void run_subtitle_tests() {
    describe("Caption files (SRT/VTT, ADR-0017)", {
        it("read SRT with a BOM, CRLF and position settings",
           { expect(reads_srt()).toBeTruthy(); });
        it("read WebVTT headers, notes, identifiers and settings",
           { expect(reads_vtt()).toBeTruthy(); });
        it("sort cues and cut overlaps at the next start",
           { expect(cuts_overlaps()).toBeTruthy(); });
        it("refuse bad timings and limits, naming the line",
           { expect(reports_the_failing_line()).toBeTruthy(); });
        it("write SRT and VTT that read back the same", { expect(round_trips()).toBeTruthy(); });
        it("treat a line of carriage returns as blank",
           { expect(carriage_returns_are_blank()).toBeTruthy(); });
    });
}
