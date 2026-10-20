#pragma once

// MLT speed changes: Kdenlive and melt write a timewarp producer whose resource is
// "speed:file" and whose frames are source frames / speed (melt_timewarp_23976.mlt).
#include "oma/importers/mlt.hpp"

#include <format>
#include <string>
#include <variant>

// One timewarp producer at `speed`, entry frames 2-5 of a 30 fps profile.
inline std::string warped(const char* speed, const char* pitch = "0") {
    return std::format(R"(<mlt><profile frame_rate_num="30" frame_rate_den="1"/>
<producer id="p" in="0" out="9"><property name="length">10</property>
<property name="resource">{0}:/m/a.mp4</property><property name="warp_speed">{0}</property>
<property name="warp_resource">/m/a.mp4</property><property name="warp_pitch">{1}</property>
<property name="mlt_service">timewarp</property></producer>
<playlist id="l"><entry producer="p" in="2" out="5"/></playlist>
<tractor id="t"><track producer="l"/></tractor></mlt>)",
                       speed, pitch);
}

inline const oma::ir::Clip* only_clip(const oma::Result<oma::importers::Imported>& r) {
    if (!r || r->timeline.tracks.size() != 1 || r->timeline.tracks[0].items.size() != 1)
        return nullptr;
    return std::get_if<oma::ir::Clip>(&r->timeline.tracks[0].items[0]);
}
