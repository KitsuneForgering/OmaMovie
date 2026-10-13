#include "from_ir_detail.hpp"

#include "oma/timeline/edit.hpp"

namespace oma::project {

namespace tl = oma::timeline;

namespace {
// The sequence rate: the first video clip's, whose source timebase is one frame; else 30 fps.
// A fractional-frame timebase (thousandths of a frame) names no rate a sequence could use.
FrameRate rate_of(const ir::Timeline& timeline) {
    for (const ir::Track& t : timeline.tracks) {
        for (const ir::Item& item : t.items) {
            const auto* clip = std::get_if<ir::Clip>(&item);
            if (t.kind != ir::TrackKind::Video || clip == nullptr)
                continue;
            const Rational tb = clip->source.start().timebase();
            auto fps = Rational::make(tb.den(), tb.num());
            auto rate = fps && fps->to_double_approx() <= 240.0
                            ? FrameRate::make(*fps)
                            : Result<FrameRate>(frame_rates::k30);
            return rate ? *rate : frame_rates::k30;
        }
    }
    return frame_rates::k30;
}
} // namespace

Result<Document> from_ir(const ir::Timeline& timeline, const ResolveMedia& resolve,
                         ir::ImportReport& report) {
    auto created = tl::Timeline::create(rate_of(timeline), SampleRate::make(48000).value());
    if (!created)
        return std::unexpected(created.error());
    tl::Editor editor(std::move(*created));
    Document doc;
    from_ir_detail::MediaIds ids;
    from_ir_detail::resolve_media(editor, timeline, resolve, doc, ids, report);
    for (const ir::Track& t : timeline.tracks) {
        const tl::TrackId id = editor.new_track_id();
        const auto kind =
            t.kind == ir::TrackKind::Audio ? tl::TrackKind::Audio : tl::TrackKind::Video;
        if (auto added = editor.execute(tl::edit::add_track(id, kind, t.name)); !added)
            return std::unexpected(added.error());
        if (kind == tl::TrackKind::Video && !doc.storyline.valid())
            doc.storyline = id;
        if (auto placed = from_ir_detail::place_items(editor, id, t, ids, report); !placed)
            return std::unexpected(placed.error());
    }
    if (timeline.global_start && timeline.global_start->value() != 0)
        from_ir_detail::issue(report, timeline.name, "the sequence starts at a timecode offset",
                              "OmaMovie sequences start at zero");
    doc.timeline = editor.timeline();
    return doc;
}

} // namespace oma::project
