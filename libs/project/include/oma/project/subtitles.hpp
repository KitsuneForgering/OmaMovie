#pragma once

// SubRip (.srt) and WebVTT (.vtt) caption files (ADR-0017). Untrusted input (CLAUDE.md §18):
// sizes and counts are bounded, malformed timings are refused with the line that failed, and
// parsing never allocates from a size read from the file.

#include "oma/base/error.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace oma::project {

inline constexpr std::size_t kMaxSubtitleBytes = std::size_t{16} << 20;
inline constexpr std::size_t kMaxCues = 10000;
inline constexpr std::size_t kMaxCueBytes = 1000;

// A caption as the file states it, in milliseconds (the files' resolution). `end` > `start`.
struct Cue {
    std::int64_t start_ms = 0;
    std::int64_t end_ms = 0;
    std::string text; // lines joined with '\n'; markup kept as written

    friend bool operator==(const Cue&, const Cue&) = default;
};

// Reads SRT or WebVTT (detected by a leading "WEBVTT"). Accepts a UTF-8 BOM, CRLF, missing or
// non-numeric SRT indices, VTT cue identifiers, cue settings, NOTE/STYLE/REGION blocks, and
// either ',' or '.' before the milliseconds. Cues come back sorted by start; one that runs into
// the next is cut at the next start.
[[nodiscard]] Result<std::vector<Cue>> parse_subtitles(std::string_view text);

[[nodiscard]] std::string write_srt(std::span<const Cue> cues);
[[nodiscard]] std::string write_vtt(std::span<const Cue> cues);

} // namespace oma::project
