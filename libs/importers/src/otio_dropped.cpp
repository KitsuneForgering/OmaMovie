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
    if (o["source_range"].get(range) == simdjson::SUCCESS && !range.is_null())
        c.issue(who, std::format("{} source_range not imported", what), "plays untrimmed");
    if (what == "stack" && disabled(o)) // a disabled track is imported hidden and muted
        c.issue(who, "stack disabled in the source", "imported enabled");
}

} // namespace oma::importers::otio
