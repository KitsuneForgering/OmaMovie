#include "fixture.hpp"

#include "oma_test.hpp"

using namespace timeline_test;

namespace {

// Storyline A [0, 30) from frame 0 and B [30, 60) from frame 100; a title T on the overlay at
// 40 for 10 frames, connected to B where B shows source frame 110.
struct Connected {
    Fixture fx;
    ClipId a;
    ClipId b;
    ClipId title;
};

Connected connected() {
    Connected c{make_fixture(), {}, {}, {}};
    c.a = place(c.fx, c.fx.video, 0, 0, 30);
    c.b = place(c.fx, c.fx.video, 30, 100, 30);
    c.title = place(c.fx, c.fx.overlay, 40, 0, 10);
    (void)c.fx.editor.execute(edit::connect(c.title, c.b));
    return c;
}

std::int64_t start_of(const Connected& c, ClipId id) {
    const Clip* clip = c.fx.editor.timeline().find_clip(id);
    return clip != nullptr ? clip->start_ticks() : -1;
}

bool anchored_to(const Connected& c, ClipId id, ClipId primary) {
    const Clip* clip = c.fx.editor.timeline().find_clip(id);
    return clip != nullptr && clip->anchor && clip->anchor->primary == primary;
}

bool ok(Connected& c, std::unique_ptr<Command> command) {
    return c.fx.editor.execute(std::move(command)).has_value() &&
           c.fx.editor.timeline().validate().has_value();
}

// Ripple-deleting A moves B to 0 and the title with it, in one undo entry.
bool follows_a_ripple() {
    auto c = connected();
    const bool moved =
        ok(c, edit::ripple_delete(c.a)) && start_of(c, c.b) == 0 && start_of(c, c.title) == 10;
    const bool undone = c.fx.editor.undo().has_value() && start_of(c, c.b) == 30 &&
                        start_of(c, c.title) == 40 && anchored_to(c, c.title, c.b);
    const bool redone = c.fx.editor.redo().has_value() && start_of(c, c.title) == 10;
    return moved && undone && redone;
}

// A plain start trim keeps B's content in place; a ripple start trim pulls it left; a slip
// shows earlier media under the same place. The title stays on frame 110 each time.
bool follows_the_content() {
    auto c = connected();
    const bool trim = ok(c, edit::trim_start(c.b, f(35), false)) && start_of(c, c.title) == 40;
    auto r = connected();
    const bool ripple = ok(r, edit::trim_start(r.b, f(35), true)) && start_of(r, r.b) == 30 &&
                        start_of(r, r.title) == 35;
    auto s = connected();
    const bool slip = ok(s, edit::slip(s.b, f(5))) && start_of(s, s.title) == 35;
    return trim && ripple && slip;
}

// Splitting B before the title hands the connection to the tail; after it, B keeps it.
bool follows_a_split() {
    auto c = connected();
    if (!ok(c, edit::split(c.b, f(35)))) {
        return false;
    }
    const Clip* tail = c.fx.editor.timeline().clip_at(c.fx.video, 35);
    const bool to_tail = tail != nullptr && tail->id != c.b && anchored_to(c, c.title, tail->id) &&
                         start_of(c, c.title) == 40;
    auto d = connected();
    const bool stays = ok(d, edit::split(d.b, f(45))) && anchored_to(d, d.title, d.b);
    return to_tail && stays;
}

// Removing the primary removes the title (undo brings both back); trimming the title's frame
// away keeps the title and ends the connection.
bool handles_orphans() {
    auto c = connected();
    const bool removed = ok(c, edit::remove_clip(c.b)) && start_of(c, c.title) == -1 &&
                         c.fx.editor.undo().has_value() && start_of(c, c.title) == 40 &&
                         anchored_to(c, c.title, c.b);
    auto t = connected();
    const bool trimmed = ok(t, edit::trim_end(t.b, f(38), false)) && start_of(t, t.title) == 40 &&
                         !t.fx.editor.timeline().find_clip(t.title)->anchor;
    return removed && trimmed;
}

// Moving the title itself reconnects it where it lands on B, or lets go off B.
bool moving_the_dependent() {
    auto c = connected();
    const bool on =
        ok(c, edit::move_clip(c.title, c.fx.overlay, f(50))) && anchored_to(c, c.title, c.b);
    const bool off = ok(c, edit::move_clip(c.title, c.fx.overlay, f(70))) &&
                     !c.fx.editor.timeline().find_clip(c.title)->anchor;
    return on && off;
}

// A follow move that would overlap another clip fails the whole edit, history untouched.
bool overlap_rejects_the_edit() {
    auto c = connected();
    (void)place(c.fx, c.fx.overlay, 12, 0, 8);
    const std::string_view last = c.fx.editor.undo_name();
    const bool rejected = !c.fx.editor.execute(edit::ripple_delete(c.a)).has_value();
    return rejected && start_of(c, c.b) == 30 && start_of(c, c.title) == 40 &&
           c.fx.editor.undo_name() == last;
}

bool connect_rules() {
    auto c = connected();
    const ClipId other = place(c.fx, c.fx.overlay, 0, 0, 5);
    return !c.fx.editor.execute(edit::connect(c.a, c.b)).has_value() &&     // same track
           !c.fx.editor.execute(edit::connect(other, c.b)).has_value() &&   // starts off B
           !c.fx.editor.execute(edit::connect(c.b, c.title)).has_value() && // primary connected
           ok(c, edit::disconnect(c.title)) && !c.fx.editor.timeline().find_clip(c.title)->anchor;
}

} // namespace

void run_anchor_tests() {
    describe("timeline connected clips (ADR-0014)", {
        it("follow a ripple delete in one undo entry",
           { expect(follows_a_ripple()).toBeTruthy(); });
        it("stay on the primary's content through trims and slips",
           { expect(follows_the_content()).toBeTruthy(); });
        it("move to the piece of a split that shows their frame",
           { expect(follows_a_split()).toBeTruthy(); });
        it("go with a removed primary and let go of trimmed content",
           { expect(handles_orphans()).toBeTruthy(); });
        it("reconnect or let go when the dependent itself moves",
           { expect(moving_the_dependent()).toBeTruthy(); });
        it("reject an edit whose follow move would overlap",
           { expect(overlap_rejects_the_edit()).toBeTruthy(); });
        it("connect only across tracks to an unconnected primary",
           { expect(connect_rules()).toBeTruthy(); });
    });
}
