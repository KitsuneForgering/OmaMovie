#include "fcpxml_lanes.hpp"

#include <format>

namespace oma::importers::fcpxml {

void read_spine(Context& c, const xml::Element& spine) {
    ir::Track main{.name = "spine", .kind = ir::TrackKind::Video, .items = {}, .unsupported = {}};
    std::map<int, std::vector<Placed>> lanes;
    const auto base = spine.children.empty()
                          ? *RationalTime::make(0, c.frame)
                          : on_grid(c, spine.children[0].attribute("offset"), "spine");
    RationalTime cursor = *RationalTime::make(0, c.frame); // where the spine has reached
    for (const xml::Element& e : spine.children) {
        const std::string name(e.attribute("name"));
        const RationalTime offset = sub(on_grid(c, e.attribute("offset"), name), base);
        const RationalTime length = on_grid(c, e.attribute("duration"), name);
        if (e.name == "transition") {
            // Centred on the cut where the spine has reached: in before it, out after it.
            main.items.emplace_back(ir::Transition{
                .kind = name.find("Dissolve") != std::string::npos ? "SMPTE_Dissolve" : name,
                .in_offset = sub(cursor, offset),
                .out_offset = sub(add(offset, length), cursor),
                .unsupported = {opaque(e)}});
            continue;
        }
        std::optional<ir::Clip> clip;
        if (e.name == "asset-clip" || e.name == "clip")
            clip = clip_of(c, e);
        else if (e.name != "gap") {
            c.out.timeline.unsupported.push_back(opaque(e));
            issue(c.out.report, name.empty() ? e.name : name, e.name + " not imported",
                  "left as a gap");
        }
        if (clip)
            main.items.emplace_back(std::move(*clip));
        else
            main.items.emplace_back(ir::Gap{.duration = length});
        connect(c, e, offset, lanes);
        cursor = add(offset, length);
    }
    c.out.timeline.tracks.push_back(std::move(main));
    for (auto& [lane, items] : lanes) {
        ir::Track track{.name = std::format("lane {}", lane),
                        .kind = lane < 0 ? ir::TrackKind::Audio : ir::TrackKind::Video,
                        .items = {},
                        .unsupported = {}};
        lay_out(c, items, track);
        c.out.timeline.tracks.push_back(std::move(track));
    }
}

} // namespace oma::importers::fcpxml
