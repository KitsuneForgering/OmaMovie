#include "otio_detail.hpp"

#include <format>

namespace oma::importers::otio {

bool disabled(Object o) {
    bool enabled = true;
    return o["enabled"].get(enabled) == simdjson::SUCCESS && !enabled;
}

void report_container(Object o, std::string_view what, const std::string& name, Context& c) {
    const std::string who = name.empty() ? std::string(what) : name;
    for (const char* key : {"effects", "markers"}) {
        simdjson::dom::array list;
        if (o[key].get(list) == simdjson::SUCCESS && list.size() > 0)
            c.issue(who, std::format("{} {} not imported", what, key), "left out");
    }
    Element range;
    if (what == "stack" && o["source_range"].get(range) == simdjson::SUCCESS && !range.is_null())
        c.issue(who, "stack source_range not imported", "plays untrimmed");
    if (what == "stack" && disabled(o)) // a disabled track is imported hidden and muted
        c.issue(who, "stack disabled in the source", "imported enabled");
}

void report_trim(Object t, const ir::Track& track, Context& c) {
    Element range;
    if (t["source_range"].get(range) != simdjson::SUCCESS || range.is_null())
        return;
    // Kdenlive writes every track's whole extent (start 0, its content's length): no trim.
    const auto r = range_of(range, c, track.name);
    Result<RationalTime> content =
        r ? RationalTime::make(0, r->start().timebase()) : std::unexpected(r.error());
    for (const ir::Item& item : track.items) {
        if (const auto* clip = std::get_if<ir::Clip>(&item))
            content = content.and_then(
                [&](RationalTime sum) { return sum.plus(clip->source.duration()); });
        else if (const auto* gap = std::get_if<ir::Gap>(&item))
            content = content.and_then([&](RationalTime sum) { return sum.plus(gap->duration); });
    }
    if (!r || !content || r->start().value() != 0 || r->duration() < *content)
        c.issue(track.name, "track source_range (a trim) not imported", "plays untrimmed");
}

} // namespace oma::importers::otio
