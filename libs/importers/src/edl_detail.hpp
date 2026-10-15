#pragma once

// Shared pieces of the EDL reader (edl_*.cpp).

#include "oma/importers/edl.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oma::importers::edl {

// `HH:MM:SS:FF` (or `;`/`.` before the frames for drop-frame) as a frame count at `fps`
// (frames per second, rounded: 30 for 29.97). Drop-frame skips frames 0 and 1 of every minute
// that is not a multiple of ten (SMPTE 12M). Nothing for anything else.
[[nodiscard]] std::optional<std::int64_t> frames_of(std::string_view tc, std::int64_t fps,
                                                    bool drop);

// One event line: `001  REEL  V  C  [dur]  src_in src_out rec_in rec_out`.
struct Event {
    std::string number;
    std::string reel;
    std::string channels;        // V, A, A2, AA, B, A/V…
    std::string kind;            // C, D, W001, K…
    std::int64_t transition = 0; // frames, for D/W
    std::int64_t src_in = 0, src_out = 0, rec_in = 0, rec_out = 0;
    std::string clip_name;          // from `* FROM CLIP NAME:`
    std::string source_file;        // from `* SOURCE FILE:`
    std::vector<std::string> notes; // other comments and motion lines, kept opaque
    std::size_t line = 0;
};

// The words of a line, split on spaces.
[[nodiscard]] std::vector<std::string_view> words(std::string_view line);
// Removes `prefix` (and spaces after it) from the start of `s`, if it is there.
bool take(std::string_view& s, std::string_view prefix);
// One event line's words as an Event; an error names line `line`.
[[nodiscard]] Result<Event> parse_event(std::vector<std::string_view> w, std::int64_t fps,
                                        bool& drop, std::size_t line);

// The events of `text` in order, with the comments that follow each; `drop` is set when the
// header or a timecode says drop-frame. An error names the first malformed event line.
// Lines before the first event that are neither TITLE nor FCM go to `unread` (reported, kept).
[[nodiscard]] Result<std::vector<Event>> parse_events(std::string_view text, std::int64_t fps,
                                                      bool& drop, std::string& title,
                                                      std::vector<std::string>& unread);

} // namespace oma::importers::edl
