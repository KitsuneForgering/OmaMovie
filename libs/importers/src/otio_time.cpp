#include "otio_detail.hpp"

#include <cmath>
#include <format>

namespace oma::importers::otio {

namespace {
std::unexpected<Error> bad(std::string_view what, std::string_view where) {
    return make_error(ErrorCode::InvalidData, Category::Importer, std::string(what),
                      std::string(where));
}
} // namespace

Result<RationalTime> time_of(Element e, Context& c, std::string_view where) {
    double rate = 0.0;
    double value = 0.0;
    if (e["rate"].get(rate) != simdjson::SUCCESS || e["value"].get(value) != simdjson::SUCCESS ||
        !std::isfinite(value) || std::abs(value) > 1e15)
        return bad("invalid RationalTime", where);
    auto r = rate_of(rate, c, where);
    if (!r)
        return std::unexpected(r.error());
    // Fractional frames are kept in thousandths of a frame; finer residues are reported.
    const bool whole = std::abs(value - std::round(value)) < 1e-9;
    const std::int64_t scale = whole ? 1 : 1000;
    const double scaled = value * static_cast<double>(scale);
    if (!whole && std::abs(scaled - std::round(scaled)) > 1e-6)
        c.issue(std::string(where), std::format("time {} at rate {} rounded", value, rate),
                "below 1/1000 frame");
    auto tb = Rational::make(r->den(), r->num() * scale);
    if (!tb)
        return std::unexpected(tb.error());
    return RationalTime::make(static_cast<std::int64_t>(std::round(scaled)), *tb);
}

Result<TimeRange> range_of(Element e, Context& c, std::string_view where) {
    Element start_e;
    Element duration_e;
    if (e["start_time"].get(start_e) != simdjson::SUCCESS ||
        e["duration"].get(duration_e) != simdjson::SUCCESS)
        return bad("invalid TimeRange", where);
    auto start = time_of(start_e, c, where);
    auto duration = time_of(duration_e, c, where);
    if (!start || !duration)
        return std::unexpected(!start ? start.error() : duration.error());
    // One timebase: the duration's if the start converts exactly, else the start's (reported).
    if (auto finer = start->rescaled(duration->timebase(), Rounding::Nearest);
        finer && *finer == *start)
        return TimeRange::make(*finer, *duration);
    auto same = duration->rescaled(start->timebase(), Rounding::Nearest);
    if (!same)
        return std::unexpected(same.error());
    if (!(*same == *duration))
        c.issue(std::string(where), "duration and start use different rates",
                "duration rounded to the start's rate");
    return TimeRange::make(*start, *same);
}

} // namespace oma::importers::otio
