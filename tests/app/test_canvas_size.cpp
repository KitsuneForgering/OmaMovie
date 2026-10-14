#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>

#include "oma_test.hpp"

namespace {
bool size_is(const Session& s, int w, int h) {
    return s.canvasWidth() == w && s.canvasHeight() == h;
}

// The 320x180 fixture: an exact size is one undo entry, odd sizes go up to even, and a size the
// timeline refuses leaves the canvas as it was.
bool sets_an_exact_canvas_size() {
    Session s;
    s.setSilent(true);
    s.open(QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath());
    if (!wait_for([&] { return s.clips().size() == 1 && !s.importsPending(); }))
        return false;
    s.setCanvasSize(1440, 1080);
    const bool exact = size_is(s, 1440, 1080) && s.undoText() == QStringLiteral("Canvas");
    s.setCanvasSize(721, 405);
    const bool even = size_is(s, 722, 406);
    s.setCanvasSize(20000, 1080);
    const bool refused = size_is(s, 722, 406);
    s.undo();
    s.undo();
    return exact && even && refused && size_is(s, 320, 180);
}
} // namespace

void run_canvas_size_tests() {
    describe("Canvas size", {
        it("sets an exact even size in one undo entry and refuses one too large",
           { expect(sets_an_exact_canvas_size()).toBeTruthy(); });
    });
}
