#pragma once

// Stateful edit properties (M10, Docs/implementation-plan-qa-timeline.md): one generated step. It
// names clips by position, not ID, so a shrunk log still replays; an empty position is rejected.

#include "oma/timeline/editor.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace property {

// Field use per kind (times in frames; `clip` picks the media of Append/Insert/Overwrite):
//   Append, Insert, Overwrite   clip=media a=at (not Append) b=source in c=duration
//   Split   a=offset in the clip   Freeze   a=offset b=length c=ripple   Slip, Slide   a=delta
//   TrimStart, TrimEnd   a=delta of that edge c=ripple   Roll   a=delta of the clip's end
//   Move          b=track index a=start      RippleMove    a=at
//   Speed   a/b c=ripple   Ramp   a/2 to b/2   Connect   b=track index c=clip index
//   Transition   a=duration b=kind (3 is none) c=0 removes it   Detach   b=lane track index
//   AddMarker   a=at   RemoveMarker   b=marker index
//   AddCaption, SetCaption   clip=caption index (Set) a=start b=duration c=1 overlong text
//   RemoveCaption   b=caption index   Video   b=0 opacity key at a, value c/4; 1 effect a; 2 clear
enum class Kind {
    Append,
    Insert,
    Overwrite,
    Split,
    TrimStart,
    TrimEnd,
    RippleDelete,
    Remove,
    Roll,
    Slip,
    Slide,
    Move,
    RippleMove,
    Speed,
    Reverse,
    Freeze,
    Connect,
    Disconnect,
    Ramp,
    Transition,
    Detach,
    AddMarker,
    RemoveMarker,
    AddCaption,
    SetCaption,
    RemoveCaption,
    Video,
    Undo,
    Redo
};
inline constexpr int kKinds = 29;

struct Op {
    Kind kind = Kind::Undo;
    int track = 0;
    int clip = 0;
    std::int64_t a = 0;
    std::int64_t b = 0;
    std::int64_t c = 0;
};

// The step as a C++ initializer, so a minimized log pastes into a regression test.
[[nodiscard]] std::string to_string(const Op& op);
// The edit a step asks for; null for Undo and Redo.
[[nodiscard]] std::unique_ptr<oma::timeline::Command> to_command(const Op& op,
                                                                 oma::timeline::Editor& editor);

} // namespace property
