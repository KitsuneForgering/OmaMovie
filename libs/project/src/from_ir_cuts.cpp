#include "from_ir_detail.hpp"

#include "oma/timeline/edit.hpp"

#include <format>

namespace oma::project::from_ir_detail {

namespace tl = oma::timeline;

std::int64_t ticks_of(const tl::Timeline& t, const RationalTime& d, const std::string& who,
                      ir::ImportReport& r) {
    auto on_grid = d.rescaled(t.timebase(), Rounding::Nearest);
    if (on_grid && !(*on_grid == d))
        issue(r, who, "length is not on the sequence grid", "rounded to the nearest tick");
    return on_grid ? on_grid->value() : 0;
}

RationalTime played(const ir::Clip& c) {
    const RationalTime& d = c.source.duration();
    const auto timebase = Rational::make(d.timebase().num(), d.timebase().den() * c.speed.num());
    if (!timebase)
        return d;
    const auto t = RationalTime::make(d.value() * c.speed.den(), *timebase);
    return t ? *t : d;
}

std::optional<tl::Transition> dissolve_of(const tl::Timeline& timeline, const ir::Transition& t,
                                          const std::string& who, ir::ImportReport& r) {
    if (t.kind != "SMPTE_Dissolve") {
        issue(r, who, std::format("transition {} has no equivalent", t.kind),
              "replaced by a hard cut");
        return std::nullopt;
    }
    if (!(t.in_offset == t.out_offset))
        issue(r, who, "dissolve is not centred on the cut", "centred, same total length");
    const std::int64_t length =
        ticks_of(timeline, t.in_offset, who, r) + ticks_of(timeline, t.out_offset, who, r);
    return tl::Transition{.kind = tl::TransitionKind::Dissolve, .duration = timeline.at(length)};
}

} // namespace oma::project::from_ir_detail
