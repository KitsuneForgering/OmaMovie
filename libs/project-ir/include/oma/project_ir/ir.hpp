#pragma once

// ProjectIR (ADR-0008): what an importer makes of an external project, independent of both the
// external format and OmaMovie's model. Times keep the source's own timebase (CLAUDE.md §16.2);
// anything not represented is kept as an Opaque object next to where it was found (§16.3).

#include "oma/base/time.hpp"
#include "oma/project_ir/opaque.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace oma::ir {

struct MediaRef {
    std::string id;     // unique within the timeline; clips refer to it
    std::string target; // the file as the source wrote it (URL or path)
    std::optional<TimeRange> available;
    std::vector<Opaque> unsupported;
};

struct Clip {
    std::string name;
    std::string media; // a MediaRef id
    TimeRange source;  // the media range the clip plays
    std::vector<Opaque> unsupported;
};

struct Gap {
    RationalTime duration;
};

struct Transition {
    std::string kind; // the source's name for it, e.g. "SMPTE_Dissolve"
    RationalTime in_offset;
    RationalTime out_offset;
    std::vector<Opaque> unsupported;
};

using Item = std::variant<Clip, Gap, Transition>;
enum class TrackKind : std::uint8_t { Video, Audio };

struct Track {
    std::string name;
    TrackKind kind = TrackKind::Video;
    std::vector<Item> items;
    std::vector<Opaque> unsupported;
};

struct Timeline {
    std::string name;
    std::optional<RationalTime> global_start;
    std::vector<MediaRef> media;
    std::vector<Track> tracks;
    std::vector<Opaque> unsupported;
};

} // namespace oma::ir
