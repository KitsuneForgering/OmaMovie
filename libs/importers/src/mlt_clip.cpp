#include "mlt_detail.hpp"

#include <algorithm>
#include <format>
#include <limits>

namespace oma::importers::mlt {

RationalTime at(const Builder& b, std::int64_t frames, Rational speed) {
    return *RationalTime::make(frames * speed.num(),
                               *Rational::make(b.doc.rate.den(), b.doc.rate.num() * speed.den()));
}

std::optional<ir::Clip> clip_of(Builder& b, const Producer& p, std::int64_t in, std::int64_t out) {
    if (p.resource.empty() || p.service == "color" || p.service == "colour" ||
        p.resource == "black" || !p.speed.is_positive())
        return std::nullopt;
    // Untrusted frame numbers: their sums and products with the speed must stay in range.
    const std::int64_t limit = std::numeric_limits<std::int64_t>::max() / 4 / p.speed.num();
    if (std::max({p.in, in, out, p.length}) > limit)
        return std::nullopt;
    auto [m, fresh] = b.media.try_emplace(p.resource, std::format("m{}", b.media.size() + 1));
    if (fresh)
        b.out.timeline.media.push_back(
            {.id = m->second,
             .target = p.resource,
             .available = p.length > 0 ? std::optional(*TimeRange::make(at(b, 0, p.speed),
                                                                        at(b, p.length, p.speed)))
                                       : std::nullopt,
             .unsupported = {}});
    if (p.keep_pitch)
        issue(b.out.report, p.name, "timewarp pitch compensation not imported",
              "the sound's pitch follows the speed");
    return ir::Clip{.name = p.name,
                    .media = m->second,
                    .source =
                        *TimeRange::make(at(b, p.in + in, p.speed), at(b, out - in + 1, p.speed)),
                    .unsupported = p.filters,
                    .speed = p.speed};
}

std::string why_not(const Builder& b, const std::string& id) {
    if (const auto p = b.doc.producers.find(id); p != b.doc.producers.end()) {
        if (p->second.service == "timewarp" && !p->second.speed.is_positive())
            return "reverse or unreadable timewarp speed not imported";
        return p->second.resource.empty() ? "producer without a file"
                                          : p->second.service + " generator not imported";
    }
    if (b.doc.tractors.contains(id))
        return "nested sequence (a tractor used as a clip) not imported";
    return "unknown producer";
}

} // namespace oma::importers::mlt
