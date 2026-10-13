#include "fixture.hpp"
#include "property_lookup.hpp"
#include "property_ops.hpp"

namespace property {

using namespace timeline_test;

std::unique_ptr<Command> to_command(const Op& op, Editor& editor) {
    const Timeline& tl = editor.timeline();
    const Clip* clip = clip_at(tl, op.track, op.clip);
    const ClipId id = clip_id(tl, op.track, op.clip);
    const std::int64_t start = clip ? clip->start_ticks() : 0;
    const std::int64_t end = clip ? clip->end_ticks() : 0;
    const bool ripple = op.c != 0;
    switch (op.kind) {
    case Kind::Append:
    case Kind::Insert:
    case Kind::Overwrite:
        return placement(op, editor);
    case Kind::Split:
        return edit::split(id, f(start + op.a));
    case Kind::TrimStart:
        return edit::trim_start(id, f(start + op.a), ripple);
    case Kind::TrimEnd:
        return edit::trim_end(id, f(end + op.a), ripple);
    case Kind::RippleDelete:
        return edit::ripple_delete(id);
    case Kind::Remove:
        return edit::remove_clip(id);
    case Kind::Roll:
        return edit::roll(id, f(end + op.a));
    case Kind::Slip:
        return edit::slip(id, f(op.a));
    case Kind::Slide:
        return edit::slide(id, f(op.a));
    case Kind::Move:
        return edit::move_clip(id, track_id(tl, op.b), f(op.a));
    case Kind::RippleMove:
        return edit::ripple_move(id, f(op.a));
    case Kind::Speed:
        return edit::set_speed(
            id, oma::Rational::make(op.a, op.b).value_or(oma::Rational::literal(0, 1)), ripple);
    case Kind::Reverse:
        return edit::reverse(id);
    case Kind::Freeze:
        return edit::freeze_frame(id, f(start + op.a), f(op.b), ripple);
    case Kind::Connect:
        return edit::connect(id, clip_id(tl, op.b, op.c));
    case Kind::Disconnect:
        return edit::disconnect(id);
    case Kind::Ramp:
        return ramp(op, clip);
    default: // Undo and Redo go through the Editor, not a command
        return nullptr;
    }
}

} // namespace property
