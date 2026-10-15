#include "mlt_detail.hpp"

#include <format>

namespace oma::importers::mlt {

RationalTime at(const Builder& b, std::int64_t frames) {
    return *RationalTime::make(frames, *b.doc.rate.inverse());
}

std::optional<ir::Clip> clip_of(Builder& b, const Producer& p, std::int64_t in, std::int64_t out) {
    if (p.resource.empty() || p.service == "color" || p.service == "colour" ||
        p.resource == "black")
        return std::nullopt;
    auto [m, fresh] = b.media.try_emplace(p.resource, std::format("m{}", b.media.size() + 1));
    if (fresh)
        b.out.timeline.media.push_back(
            {.id = m->second,
             .target = p.resource,
             .available = p.length > 0 ? std::optional(*TimeRange::make(at(b, 0), at(b, p.length)))
                                       : std::nullopt,
             .unsupported = {}});
    return ir::Clip{.name = p.name,
                    .media = m->second,
                    .source = *TimeRange::make(at(b, p.in + in), at(b, out - in + 1)),
                    .unsupported = p.filters};
}

} // namespace oma::importers::mlt
