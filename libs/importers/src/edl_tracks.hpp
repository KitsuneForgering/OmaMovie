#pragma once

// How EDL events become ProjectIR pieces (edl_reader.cpp builds the tracks from them).

#include "edl_detail.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace oma::importers::edl {

// The IR tracks an event's channel field puts it on: V, A1, A2…; none for an unknown field.
[[nodiscard]] std::vector<std::string> tracks_of(std::string_view channels);

// A comment or motion (`M2`) line of an event, kept for a later round trip (§16.3).
[[nodiscard]] ir::Opaque note_of(const Event& e, const std::string& line);

// An event the IR cannot represent (a wipe or a key), kept with its reel and channels.
[[nodiscard]] ir::Opaque event_of(const Event& e);

// The timeline being built: tracks by name and where each has reached on the record.
struct Build {
    Imported& out;
    Rational timebase;  // one frame
    std::int64_t start; // the earliest record-in: timeline time 0
    std::map<std::string, ir::Track> tracks;
    std::map<std::string, std::int64_t> cursor;
    std::unordered_map<std::string, std::string> media; // target -> media id
};

// Appends `e` to track `name`: a gap up to its record-in, a dissolve or a reported wipe/key,
// then the clip; the outgoing side of a dissolve (a zero-length cut) adds nothing.
void place(Build& b, const Event& e, const std::string& name);

// The event's clip: its media shared by source file, clip name or reel, and its notes kept and
// reported (an M2 speed change plays at normal speed).
[[nodiscard]] ir::Clip clip_of(Build& b, const Event& e);

void issue(ir::ImportReport& r, std::string object, std::string reason, std::string impact);

// Header lines the reader did not understand: reported and kept on the timeline (§16.3).
void keep_unread(Imported& out, std::vector<std::string> lines);

} // namespace oma::importers::edl
