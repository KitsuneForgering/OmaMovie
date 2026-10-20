#include "fixture.hpp"
#include "property_invariants.hpp"
#include "property_lookup.hpp"

#include <format>
#include <set>
#include <string>
#include <utility>

namespace property {

using namespace timeline_test;

MarkerId marker_id(const Timeline& tl, std::int64_t index) {
    const auto markers = tl.markers();
    return index >= 0 && std::cmp_less(index, markers.size())
               ? markers[static_cast<std::size_t>(index)].id
               : MarkerId{};
}

CaptionId caption_id(const Timeline& tl, std::int64_t index) {
    const auto captions = tl.captions();
    return index >= 0 && std::cmp_less(index, captions.size())
               ? captions[static_cast<std::size_t>(index)].id
               : CaptionId{};
}

std::unique_ptr<Command> caption(const Op& op, Editor& editor) {
    Caption c{.id = op.kind == Kind::AddCaption ? editor.new_caption_id()
                                                : caption_id(editor.timeline(), op.clip),
              .start = f(op.a),
              .duration = f(op.b),
              .text = op.c != 0 ? std::string(kMaxCaptionBytes + 1, 'x') : "caption"};
    return op.kind == Kind::AddCaption ? edit::add_caption(std::move(c))
                                       : edit::set_caption(std::move(c));
}

std::string broken_annotations(const Timeline& tl) {
    std::set<std::uint64_t> markers;
    oma::RationalTime previous;
    for (const Marker& m : tl.markers()) {
        if (!m.id.valid() || !markers.insert(m.id.value()).second || !tl.to_ticks(m.time) ||
            m.time.value() < 0)
            return std::format("marker {}: invalid, duplicate, off the grid or before the start",
                               m.id.value());
        if (markers.size() > 1 && m.time < previous)
            return std::format("marker {}: out of time order", m.id.value());
        previous = m.time;
    }
    std::set<std::uint64_t> ids;
    std::int64_t end = 0;
    for (const Caption& c : tl.captions()) {
        if (!c.id.valid() || !ids.insert(c.id.value()).second)
            return std::format("caption {}: invalid or duplicate ID", c.id.value());
        if (!tl.to_ticks(c.start) || !tl.to_ticks(c.duration) || c.start.value() < 0 ||
            c.duration.value() <= 0 || c.text.size() > kMaxCaptionBytes)
            return std::format("caption {}: off the grid, empty or too long", c.id.value());
        if (c.start.value() < end)
            return std::format("caption {}: overlaps or precedes the one before", c.id.value());
        end = c.start.value() + c.duration.value();
    }
    return {};
}

} // namespace property
