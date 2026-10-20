#include "fcpxml_detail.hpp"

#include <format>

namespace oma::importers::fcpxml {

std::optional<ir::Clip> clip_of(Context& c, const xml::Element& e) {
    // A `clip` wraps its media in a `video` or `audio` child whose own offset maps clip time to
    // the asset: source = clip start - child offset + child start.
    const xml::Element* media = &e;
    if (e.name == "clip")
        for (const xml::Element& child : e.children)
            if ((child.name == "video" || child.name == "audio") && media == &e)
                media = &child;
    const std::string name(e.attribute("name"));
    if (e.attribute("enabled") == "0") { // Final Cut's disabled clip: it shows and sounds nothing
        issue(c.out.report, name, "clip disabled in the source",
              "left as a gap on the spine, left out when connected");
        return std::nullopt;
    }
    const auto id = media_of(c, media->attribute("ref"));
    if (!id) {
        issue(c.out.report, name, "clip without a readable asset", "left as a gap");
        return std::nullopt;
    }
    auto source = on_grid(c, e.attribute("start").empty() ? "0s" : e.attribute("start"), name);
    if (media != &e)
        source = source.minus(on_grid(c, media->attribute("offset"), name))
                     .and_then([&](RationalTime t) {
                         return t.plus(on_grid(c, media->attribute("start"), name));
                     })
                     .value_or(source); // an out-of-range sum keeps the clip's own start
    ir::Clip clip{.name = name,
                  .media = *id,
                  .source = *TimeRange::make(source, on_grid(c, e.attribute("duration"), name)),
                  .unsupported = {}};
    bool effects = false;
    bool retimed = false; // a speed change, ramp or rate conform: its timing is not read
    for (const xml::Element& child : e.children) {
        const bool effect = child.name.starts_with("filter-") || child.name.starts_with("adjust-");
        const bool timing = child.name == "timeMap" || child.name == "conform-rate";
        if (effect || timing)
            clip.unsupported.push_back(opaque(child));
        effects = effects || effect;
        retimed = retimed || timing;
    }
    if (effects)
        issue(c.out.report, name, "effects and adjustments not imported", "kept in the import");
    if (retimed)
        issue(c.out.report, name, "retiming (timeMap, conform-rate) not imported",
              "plays at normal speed for its length");
    return clip;
}

} // namespace oma::importers::fcpxml
