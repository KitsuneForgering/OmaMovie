#include "from_ir_dissolve.hpp"

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
    const std::int64_t length =
        ticks_of(timeline, t.in_offset, who, r) + ticks_of(timeline, t.out_offset, who, r);
    return tl::Transition{.kind = tl::TransitionKind::Dissolve, .duration = timeline.at(length)};
}

std::optional<Edit> centring_roll(const tl::Timeline& timeline, tl::ClipId into,
                                  const ir::Transition& t, const std::string& who,
                                  ir::ImportReport& r) {
    const std::int64_t shift =
        (ticks_of(timeline, t.in_offset, who, r) - ticks_of(timeline, t.out_offset, who, r)) / 2;
    const tl::Clip* right = timeline.find_clip(into);
    const tl::Track* track = timeline.track_of(into);
    const tl::Clip* left =
        right && track ? timeline.clip_at(track->id, right->start_ticks() - 1) : nullptr;
    if (shift == 0 || left == nullptr || left->end_ticks() != right->start_ticks())
        return std::nullopt;
    return [l = left->id, cut = timeline.at(right->start_ticks() - shift)] {
        return tl::edit::roll(l, cut);
    };
}

} // namespace oma::project::from_ir_detail
