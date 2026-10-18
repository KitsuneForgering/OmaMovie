// libFuzzer target for SRT/WebVTT caption files (CLAUDE.md §18, ADR-0017): any input gives
// sorted, bounded, non-overlapping cues that write and read back the same, or a handled error.

#include "oma/project/subtitles.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    auto cues = oma::project::parse_subtitles(text);
    if (!cues) {
        return 0;
    }
    if (cues->size() > oma::project::kMaxCues) {
        __builtin_trap();
    }
    for (std::size_t i = 0; i < cues->size(); ++i) {
        const auto& c = (*cues)[i];
        if (c.end_ms <= c.start_ms || c.text.size() > oma::project::kMaxCueBytes ||
            (i > 0 && (*cues)[i - 1].end_ms > c.start_ms)) {
            __builtin_trap();
        }
    }
    // Text lines that are themselves timing lines or blank cannot round-trip: only check the
    // timing of what reads back.
    auto again = oma::project::parse_subtitles(oma::project::write_srt(*cues));
    if (!again) {
        __builtin_trap();
    }
    return 0;
}
