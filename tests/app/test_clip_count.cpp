// Session::clipCount, read by QML bindings instead of the whole clip list (5,000-clip edits).
#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>

#include "oma_test.hpp"

namespace {
// The count follows the storyline through edits and their undo.
bool count_follows_edits() {
    Session s;
    s.setSilent(true);
    if (s.clipCount() != 0)
        return false;
    s.open(QFileInfo("tests/fixtures/generated/h264_long_gop.mp4").absoluteFilePath());
    if (!wait_for([&] { return s.clips().size() == 1 && !s.importsPending(); }))
        return false;
    bool same = s.clipCount() == 1;
    s.seek(4.0);
    s.splitAtPlayhead();
    same = same && s.clipCount() == 2 && s.clips().size() == 2;
    s.undo();
    same = same && s.clipCount() == 1 && s.clips().size() == 1;
    s.newProject();
    return same && s.clipCount() == 0;
}
} // namespace

void run_clip_count_tests() {
    describe("Clip count", {
        it("follows the storyline through edits, undo and a new project",
           { expect(count_follows_edits()).toBeTruthy(); });
    });
}
