#include "fcpxml_lanes.hpp"

#include <algorithm>
#include <charconv>

namespace oma::importers::fcpxml {

void connect(Context& c, const xml::Element& e, const RationalTime& offset,
             std::map<int, std::vector<Placed>>& lanes) {
    // Connected clips sit in the parent's own time: timeline = offset + (child - parent start).
    const std::string name(e.attribute("name"));
    const RationalTime start = on_grid(c, e.attribute("start"), name);
    for (const xml::Element& child : e.children) {
        const std::string_view l = child.attribute("lane");
        int lane = 0;
        if (l.empty() || std::from_chars(l.begin(), l.end(), lane).ptr != l.end() || lane == 0)
            continue;
        if (child.name != "asset-clip" && child.name != "clip") {
            const std::string what(child.attribute("name"));
            c.out.timeline.unsupported.push_back(opaque(child));
            issue(c.out.report, what.empty() ? child.name : what,
                  "connected " + child.name + " not imported", "left out");
        } else if (auto clip = clip_of(c, child)) {
            const auto at = add(offset, sub(on_grid(c, child.attribute("offset"), name), start));
            lanes[lane].push_back({.at = at, .clip = std::move(*clip)});
        }
    }
}

void lay_out(Context& c, std::vector<Placed>& items, ir::Track& track) {
    std::ranges::sort(items, {}, [](const Placed& p) { return p.at; });
    RationalTime cursor = *RationalTime::make(0, c.frame);
    for (Placed& p : items) {
        if (p.at < cursor) {
            issue(c.out.report, p.clip.name, "connected clip overlaps another on its lane",
                  "left out");
            continue;
        }
        if (cursor < p.at)
            track.items.emplace_back(ir::Gap{.duration = sub(p.at, cursor)});
        cursor = add(p.at, p.clip.source.duration());
        track.items.emplace_back(std::move(p.clip));
    }
}

} // namespace oma::importers::fcpxml
