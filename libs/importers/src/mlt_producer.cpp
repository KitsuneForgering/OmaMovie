#include "mlt_detail.hpp"

#include <filesystem>

namespace oma::importers::mlt {

Producer producer_of(const xml::Element& e, const Document& doc, std::string_view root) {
    Producer p{.resource = property(e, "resource"),
               .service = property(e, "mlt_service"),
               .name = property(e, "kdenlive:clipname"),
               .in = 0,
               .length = -1,
               .filters = {}};
    if (p.service == "timewarp") { // Kdenlive's speed change: resource is "speed:file"
        p.resource = property(e, "warp_resource");
        const auto speed = speed_of(property(e, "warp_speed"));
        p.speed = speed && speed->is_positive() ? *speed : Rational();
        p.keep_pitch = property(e, "warp_pitch") == "1";
    }
    const std::filesystem::path file(p.resource);
    if (!p.resource.empty() && file.is_relative() && !root.empty() && p.service != "color" &&
        p.service != "colour")
        p.resource = (std::filesystem::path(root) / file).string();
    if (p.name.empty())
        p.name = file.filename().string();
    p.in = frames_of(e.attribute("in"), doc).value_or(0);
    p.length = frames_of(property(e, "length"), doc).value_or(-1);
    for (const xml::Element& c : e.children)
        if (c.name == "filter")
            p.filters.push_back(opaque(c));
    return p;
}

Playlist playlist_of(const xml::Element& e, const Document& doc) {
    Playlist p;
    p.audio = property(e, "kdenlive:audio_track") == "1";
    for (const xml::Element& c : e.children) {
        if (c.name == "entry")
            p.entries.push_back({.producer = std::string(c.attribute("producer")),
                                 .in = frames_of(c.attribute("in"), doc).value_or(0),
                                 .out = frames_of(c.attribute("out"), doc).value_or(-1)});
        else if (c.name == "blank")
            p.entries.push_back({.producer = {},
                                 .in = 0,
                                 .out = frames_of(c.attribute("length"), doc).value_or(0) - 1});
    }
    return p;
}

} // namespace oma::importers::mlt
