#pragma once

// A small ProjectIR for the converter tests, and a resolver that finds only "m1".

#include "oma/project/from_ir.hpp"

namespace from_ir_sample {

namespace ir = oma::ir;

inline oma::RationalTime f24(std::int64_t frames) {
    return oma::RationalTime::make(frames, oma::Rational::make(1, 24).value()).value();
}

inline ir::Clip clip(const char* name, const char* media, std::int64_t in, std::int64_t frames) {
    return {.name = name,
            .media = media,
            .source = oma::TimeRange::make(f24(in), f24(frames)).value(),
            .unsupported = {}};
}

// V1: a (48 frames), a 12-frame gap, a centred 12-frame dissolve into b (24 frames), then a
// clip of missing media (24 frames) and c. m1 resolves to 10 s of 24 fps video with sound.
inline ir::Timeline sample() {
    ir::Timeline t;
    t.media = {{.id = "m1", .target = "a.mov", .available = std::nullopt, .unsupported = {}},
               {.id = "m2", .target = "gone.mov", .available = std::nullopt, .unsupported = {}}};
    ir::Track v1{.name = "V1", .kind = ir::TrackKind::Video, .items = {}, .unsupported = {}};
    v1.items = {
        clip("a", "m1", 0, 48),
        ir::Gap{.duration = f24(12)},
        ir::Transition{
            .kind = "SMPTE_Dissolve", .in_offset = f24(6), .out_offset = f24(6), .unsupported = {}},
        clip("b", "m1", 100, 24),
        clip("lost", "m2", 0, 24),
        clip("c", "m1", 200, 24)};
    t.tracks.push_back(std::move(v1));
    return t;
}

inline std::optional<oma::project::MediaRef> resolve(const ir::MediaRef& m) {
    if (m.id != "m1")
        return std::nullopt;
    oma::project::MediaRef found;
    found.info = {.id = {},
                  .start = f24(0),
                  .duration = f24(240),
                  .has_video = true,
                  .has_audio = true,
                  .still = false};
    found.path = "/media/a.mov";
    found.name = "a.mov";
    return found;
}

} // namespace from_ir_sample
