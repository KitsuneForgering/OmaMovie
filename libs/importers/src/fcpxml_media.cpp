#include "fcpxml_detail.hpp"

#include <format>

namespace oma::importers::fcpxml {

// The IR media of asset `ref`, made on first use with the asset's own range as available.
std::optional<std::string> media_of(Context& c, std::string_view ref) {
    const auto a = c.assets.find(std::string(ref));
    if (a == c.assets.end() || a->second.src.empty())
        return std::nullopt;
    auto [m, fresh] = c.media.try_emplace(a->first, std::format("m{}", c.media.size() + 1));
    if (fresh) {
        const auto start = on_grid(c, a->second.start.empty() ? "0s" : a->second.start, a->first);
        const auto length = on_grid(c, a->second.duration, a->first);
        c.out.timeline.media.push_back(
            {.id = m->second,
             .target = a->second.src,
             .available =
                 length.value() > 0 ? std::optional(*TimeRange::make(start, length)) : std::nullopt,
             .unsupported = {}});
    }
    return m->second;
}

} // namespace oma::importers::fcpxml
