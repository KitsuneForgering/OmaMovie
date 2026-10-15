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
    for (const xml::Element& child : e.children)
        if (child.name.starts_with("filter-") || child.name.starts_with("adjust-"))
            clip.unsupported.push_back(opaque(child));
    if (!clip.unsupported.empty())
        issue(c.out.report, name, "effects and adjustments not imported", "kept in the import");
    return clip;
}

} // namespace oma::importers::fcpxml
