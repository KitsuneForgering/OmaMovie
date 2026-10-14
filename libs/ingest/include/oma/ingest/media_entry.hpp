#pragma once

// How a probed file enters the timeline's media table: shared by the library import and the
// OpenTimelineIO import, so both place clips against the same ranges.

#include "oma/media/format.hpp"
#include "oma/timeline/model.hpp"

#include <optional>

namespace oma::ingest {

struct MediaEntry {
    oma::timeline::MediaInfo info;
    bool audio_only = false;                 // no video stream: placed on the audio lanes
    std::optional<oma::RationalTime> length; // what the container or stream declares
};

// None when the file has neither a video nor an audio stream.
[[nodiscard]] std::optional<MediaEntry> media_entry(const oma::media::MediaInfo& probed,
                                                    oma::timeline::MediaId id);

} // namespace oma::ingest
