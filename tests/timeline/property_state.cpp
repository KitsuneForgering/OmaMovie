#include "property_state.hpp"

#include <format>

namespace property {

using namespace oma::timeline;

namespace {

std::string t(const oma::RationalTime& time) {
    return std::format("{}@{}/{}", time.value(), time.timebase().num(), time.timebase().den());
}

std::string map(const TimeMap& m) {
    std::string out =
        m.is_constant() ? std::format(" x{}/{}", m.speed().num(), m.speed().den()) : "";
    for (const TimeSegment& s : m.segments())
        out += std::format(" {}:{}:{}/{}>{}/{}", static_cast<int>(s.kind), s.length, s.from.num(),
                           s.from.den(), s.to.num(), s.to.den());
    return out;
}

std::string clip_line(const Clip& c) {
    std::string out = std::format("  clip {} media {} {}+{} in {} map{}{}", c.id.value(),
                                  c.media.value(), t(c.start), t(c.duration), t(c.source_in),
                                  map(c.time_map), c.audio_detached ? " detached" : "");
    if (c.transition_in)
        out += std::format(" transition {} {}", static_cast<int>(c.transition_in->kind),
                           t(c.transition_in->duration));
    if (c.anchor)
        out += std::format(" anchor {} {}", c.anchor->primary.value(), t(c.anchor->source));
    if (c.title)
        out += " title " + c.title->text;
    return out + "\n";
}

} // namespace

State canonical(const Timeline& tl) {
    State s;
    s.text = std::format("canvas {}x{}\n", tl.canvas_width(), tl.canvas_height());
    for (const Track& track : tl.tracks()) {
        s.text += std::format("track {} kind {} {} muted {} hidden {}\n", track.id.value(),
                              static_cast<int>(track.kind), track.name, track.muted, track.hidden);
        for (const Clip& c : track.clips) {
            s.text += clip_line(c);
            s.video.push_back(c.video);
            s.audio.push_back(c.audio);
        }
    }
    for (const Marker& m : tl.markers())
        s.text += std::format("marker {} {} {}\n", m.id.value(), t(m.time), m.name);
    for (const Caption& c : tl.captions())
        s.text +=
            std::format("caption {} {}+{} {}\n", c.id.value(), t(c.start), t(c.duration), c.text);
    return s;
}

} // namespace property
