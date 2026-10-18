#include "property_lookup.hpp"

#include "fixture.hpp"

namespace property {

using namespace timeline_test;

const Track* track_at(const Timeline& tl, std::int64_t index) {
    const auto tracks = tl.tracks();
    return index >= 0 && static_cast<std::size_t>(index) < tracks.size()
               ? &tracks[static_cast<std::size_t>(index)]
               : nullptr;
}

const Clip* clip_at(const Timeline& tl, std::int64_t track, std::int64_t index) {
    const Track* t = track_at(tl, track);
    return t && index >= 0 && static_cast<std::size_t>(index) < t->clips.size()
               ? &t->clips[static_cast<std::size_t>(index)]
               : nullptr;
}

TrackId track_id(const Timeline& tl, std::int64_t index) {
    const Track* t = track_at(tl, index);
    return t ? t->id : TrackId{};
}

ClipId clip_id(const Timeline& tl, std::int64_t track, std::int64_t index) {
    const Clip* c = clip_at(tl, track, index);
    return c ? c->id : ClipId{};
}

namespace {

edit::ClipSource source(std::int64_t media, std::int64_t in, std::int64_t duration) {
    edit::ClipSource s = camera(in, duration);
    if (media == 1) {
        s.media = kMusic;
        s.source_in = oma::RationalTime::make(in * 1600, q(1, 48000)).value();
    } else if (media == 2) {
        s.media = kPicture;
        s.source_in = {};
    }
    return s;
}

} // namespace

std::unique_ptr<Command> placement(const Op& op, Editor& editor) {
    const TrackId tid = track_id(editor.timeline(), op.track);
    const ClipId id = editor.new_clip_id();
    const edit::ClipSource s = source(op.clip, op.b, op.c);
    if (op.kind == Kind::Append)
        return edit::append(tid, id, s);
    return op.kind == Kind::Insert ? edit::insert(tid, id, f(op.a), s)
                                   : edit::overwrite(tid, id, f(op.a), s);
}

} // namespace property
