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

std::string why_not(const Builder& b, const std::string& id) {
    if (const auto p = b.doc.producers.find(id); p != b.doc.producers.end())
        return p->second.resource.empty() ? "producer without a file"
                                          : p->second.service + " generator not imported";
    if (b.doc.tractors.contains(id))
        return "nested sequence (a tractor used as a clip) not imported";
    return "unknown producer";
}

} // namespace oma::importers::mlt
