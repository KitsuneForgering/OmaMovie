#pragma once

// The MLT document as the importer sees it: elements indexed by id (mlt_index.cpp).

#include "oma/importers/mlt.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace oma::importers::mlt {

struct Producer {
    std::string resource;     // the file, joined to the document's root when relative
    std::string service;      // mlt_service: avformat, color, …
    std::string name;         // kdenlive:clipname, else the file name
    std::int64_t in = 0;      // entry points count from here
    std::int64_t length = -1; // frames, when the producer says
    std::vector<ir::Opaque> filters;
    // A timewarp's warp_speed: its frames (in, length, entry points) are source frames / speed;
    // zero when the speed is reverse or unreadable, which the importer does not place.
    Rational speed = Rational::literal(1, 1);
    bool keep_pitch = false; // warp_pitch: compensated sound, which the importer reports
};

struct Entry {
    std::string producer; // empty: a blank
    std::int64_t in = 0;
    std::int64_t out = -1; // inclusive; a blank's length is out + 1
};

struct Playlist {
    std::vector<Entry> entries;
    bool audio = false; // kdenlive:audio_track or hide="video"
};

struct TrackRef {
    std::string producer;
    bool audio = false;  // hide="video"
    bool off = false;    // hide="both": neither picture nor sound
    std::int64_t in = 0; // a mix's tracks cut their producers
    std::int64_t out = -1;
};

struct Tractor {
    std::vector<TrackRef> tracks;
    std::vector<ir::Opaque> transitions;
    std::int64_t length = 0; // out - in + 1
};

struct Document {
    Rational rate;
    std::int64_t fps = 25; // rounded frames per second, for clock and timecode values
    std::map<std::string, Producer> producers;
    std::map<std::string, Playlist> playlists;
    std::map<std::string, Tractor> tractors;
    std::string main; // the last tractor: MLT plays the last service of a document
};

} // namespace oma::importers::mlt
