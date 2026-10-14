#include "property_gen.hpp"

namespace property {

using namespace oma::timeline;

Op generate(Rng& rng, const Timeline& tl) {
    Op op;
    op.kind = static_cast<Kind>(rng.between(0, kKinds - 1));
    const auto tracks = static_cast<std::int64_t>(tl.tracks().size());
    // One position in ten past the last track or clip: the edit must reject it.
    const bool past = rng.between(0, 9) == 0;
    op.track = static_cast<int>(rng.between(0, past ? tracks : tracks - 1));
    const auto all = tl.tracks(); // a view of the timeline's tracks
    const auto& clips = op.track < tracks ? all[static_cast<std::size_t>(op.track)].clips
                                          : all.front().clips;
    const auto count = static_cast<std::int64_t>(clips.size());
    op.clip = static_cast<int>(rng.between(0, count == 0 || past ? count : count - 1));
    const Clip* clip = op.clip < count ? &clips[static_cast<std::size_t>(op.clip)] : nullptr;
    const Extent extent{.clip = clip ? clip->duration.value() : 30,
                        .timeline = tl.to_ticks(tl.duration()).value_or(0),
                        .tracks = tracks};
    choose_values(rng, op, extent);
    aim(rng, tl, op);
    return op;
}

} // namespace property
