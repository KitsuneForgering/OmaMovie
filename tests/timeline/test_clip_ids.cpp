// Clip IDs stay unique across tracks however a clip arrives: inserting checks a bound before
// searching every track, so an ID chosen by the caller must raise that bound.
#include "fixture.hpp"

#include <memory>
#include <vector>

#include "oma_test.hpp"

using namespace timeline_test;

namespace {

bool overwrite_with(Fixture& fx, TrackId track, ClipId id, std::int64_t at) {
    return fx.editor.execute(edit::overwrite(track, id, f(at), camera(0, 10))).has_value();
}

// An ID in use on another track, one used and removed (then restored by undo), and one far
// above any allocated ID: each is refused the second time it is used.
bool ids_stay_unique() {
    Fixture fx = make_fixture();
    const ClipId a = place(fx, fx.video, 0, 0, 10);
    if (!a.valid() || overwrite_with(fx, fx.overlay, a, 40))
        return false;
    const ClipId b = place(fx, fx.video, 20, 0, 10);
    if (!fx.editor.execute(edit::remove_clip(b)) || !fx.editor.undo() ||
        overwrite_with(fx, fx.overlay, b, 60))
        return false;
    const ClipId far{1'000'000'000};
    if (!overwrite_with(fx, fx.overlay, far, 80) || overwrite_with(fx, fx.audio, far, 0))
        return false;
    // Inside a transaction a duplicate must be refused by the insert itself: let through onto
    // the original's track, the failing step after it reverts by erasing that ID from the track,
    // which would take the original clip with it.
    std::vector<std::unique_ptr<Command>> steps;
    steps.push_back(edit::overwrite(fx.overlay, far, f(200), camera(0, 10)));
    steps.push_back(edit::remove_clip(ClipId{999})); // no such clip: the transaction fails
    if (fx.editor.execute(edit::transaction("dup", std::move(steps))))
        return false;
    const Clip* kept = fx.editor.timeline().find_clip(far);
    if (kept == nullptr || kept->start != f(80) ||
        fx.editor.timeline().find_track(fx.overlay)->clips.size() != 1)
        return false;
    const ClipId next = fx.editor.new_clip_id(); // allocation stays below the caller's ID
    return overwrite_with(fx, fx.overlay, next, 100) && next != far &&
           fx.editor.timeline().validate().has_value();
}

// Moving a clip checks its destination against the clips there, at either edge and around them,
// but not against its own old place; touching a neighbour is allowed.
bool ranges_stay_free() {
    Fixture fx = make_fixture();
    const ClipId a = place(fx, fx.video, 10, 0, 10); // 10-20
    const ClipId b = place(fx, fx.video, 30, 0, 10); // 30-40
    if (!a.valid() || !b.valid())
        return false;
    Editor& ed = fx.editor;
    const auto moves = [&](std::int64_t to) {
        return ed.execute(edit::move_clip(b, fx.video, f(to))).has_value();
    };
    return !moves(15) && !moves(5) && !moves(11) && moves(34) && moves(20) && !moves(19) &&
           moves(26) && ed.timeline().validate().has_value();
}

} // namespace

void run_clip_id_tests() {
    describe("Clip IDs and free ranges", {
        it("keeps clip IDs unique across tracks, undo and caller-chosen IDs",
           { expect(ids_stay_unique()).toBeTruthy(); });
        it("refuses overlapping clips and accepts touching ones",
           { expect(ranges_stay_free()).toBeTruthy(); });
    });
}
