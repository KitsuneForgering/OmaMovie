#pragma once

// The structured report every import produces (CLAUDE.md §16.4), shown by the UI and by
// `oma-project`; importers and the converter both append to it.

#include <cstdint>
#include <string>
#include <vector>

namespace oma::ir {

// Support levels (CLAUDE.md §16.5), declared per format and version in Docs/interop.md.
enum class SupportLevel : std::uint8_t {
    Recognized = 0, // file recognized and version identified
    Media = 1,      // referenced media and clips
    Edits = 2,      // tracks, cuts, transitions
    Transforms = 3, // transforms, audio gain/fades, keyframes
    Effects = 4,    // effects with a mapped equivalent
    RoundTrip = 5,
};

struct ImportIssue {
    std::string object; // the affected object, as the source names it
    std::string reason;
    SupportLevel level = SupportLevel::Recognized; // the level this object reached
    std::string impact; // e.g. "transition replaced by a hard cut; duration preserved"
};

struct ImportReport {
    std::string format;  // e.g. "OpenTimelineIO JSON"
    std::string version; // the source's schema or application version
    SupportLevel reached = SupportLevel::Recognized;
    std::vector<ImportIssue> issues;
};

} // namespace oma::ir
