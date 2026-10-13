#pragma once

// Shared helpers for the OTIO import tests.

#include "oma/importers/otio.hpp"

#include <fstream>
#include <sstream>
#include <string>

inline std::string read_fixture(const char* name) {
    std::ifstream in(std::string("tests/importers/fixtures/") + name);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

// Frames at 24000/1001 fps, the fixture's rate.
inline oma::RationalTime ntsc_frames(std::int64_t frames, std::int64_t per_frame = 1) {
    return oma::RationalTime::make(frames, oma::Rational::make(1001, 24000 * per_frame).value())
        .value();
}
