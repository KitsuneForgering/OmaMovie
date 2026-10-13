#include "edge_frames.hpp"

#include "oma/timeline/evaluate.hpp"

namespace tl = oma::timeline;

namespace {
// The frame of `c` shown at sequence `ticks` (its own first or last frame).
EdgeSide side(const tl::Timeline& t, const tl::Clip* c, std::int64_t ticks,
              const MediaPaths& paths) {
    if (c == nullptr || c->title)
        return {};
    const auto path = paths.find(c->media.value());
    auto source = tl::source_time(t, *c, ticks);
    if (path == paths.end() || !source)
        return {};
    return {.path = path->second, .source = *source};
}
} // namespace

std::optional<std::array<EdgeSide, 2>> edge_sides(const tl::Timeline& t, tl::ClipId clip,
                                                  EdgeEdit edit, std::int64_t ticks_per_frame,
                                                  const MediaPaths& paths) {
    const tl::Track* track = t.track_of(clip);
    if (track == nullptr)
        return std::nullopt;
    const auto& clips = track->clips;
    const auto at = std::ranges::find(clips, clip, &tl::Clip::id) - clips.begin();
    const tl::Clip& c = clips[static_cast<std::size_t>(at)];
    // Neighbours count only when they touch the clip: across a gap there is no cut to judge.
    const tl::Clip* before =
        at > 0 && clips[static_cast<std::size_t>(at) - 1].end_ticks() == c.start_ticks()
            ? &clips[static_cast<std::size_t>(at) - 1]
            : nullptr;
    const tl::Clip* after =
        static_cast<std::size_t>(at) + 1 < clips.size() &&
                clips[static_cast<std::size_t>(at) + 1].start_ticks() == c.end_ticks()
            ? &clips[static_cast<std::size_t>(at) + 1]
            : nullptr;
    switch (edit) {
    case EdgeEdit::Roll:
        return std::array{side(t, &c, c.end_ticks() - ticks_per_frame, paths),
                          side(t, after, c.end_ticks(), paths)};
    case EdgeEdit::Slip:
        return std::array{side(t, &c, c.start_ticks(), paths),
                          side(t, &c, c.end_ticks() - ticks_per_frame, paths)};
    case EdgeEdit::Slide:
        return std::array{side(t, before, c.start_ticks() - ticks_per_frame, paths),
                          side(t, after, c.end_ticks(), paths)};
    }
    return std::nullopt;
}
