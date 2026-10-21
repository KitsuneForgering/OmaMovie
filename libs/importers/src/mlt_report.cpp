#include "mlt_detail.hpp"

#include <algorithm>

namespace oma::importers::mlt {

bool is_a(const ir::Opaque& o, std::string_view element) {
    return std::ranges::any_of(
        o.fields, [&](const auto& f) { return f.first == "element" && f.second == element; });
}

bool inert(const ir::Opaque& o) {
    return o.schema == "audiolevel" || std::ranges::any_of(o.fields, [](const auto& f) {
               return f.first == "disable" && f.second == "1";
           });
}

void issue(ir::ImportReport& r, std::string object, std::string reason, std::string impact) {
    r.issues.push_back({.object = std::move(object),
                        .reason = std::move(reason),
                        .level = ir::SupportLevel::Edits,
                        .impact = std::move(impact)});
}

} // namespace oma::importers::mlt
