#include "session.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>

#include <cmath>
#include <functional>

#include "oma_test.hpp"

namespace {

// Runs the event loop until `done` holds or `ms` pass (imports finish on job workers and
// report back through queued calls).
bool wait_until(const std::function<bool()>& done, int ms = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return done();
}

QString fixture(const char* name) {
    return QFileInfo(QStringLiteral("tests/fixtures/generated/") + QString::fromLatin1(name))
        .absoluteFilePath();
}

// A session with one 1 s 30 fps clip on the storyline (the fixture), or nothing on failure.
bool load_clip(Session& s) {
    s.setSilent(true);
    s.open(fixture("h264_30fps_aac.mp4"));
    return wait_until([&] { return s.clips().size() == 1 && !s.importsPending(); });
}

double clip_seconds(const Session& s, qsizetype index) {
    return s.clips().at(index).toMap().value("duration").toDouble();
}

// Source viewer (M6 pilot): a marked range is what Add uses; the frame under the playhead at
// Mark out is kept; undo removes the clip; marks persist for the session and are visible.
bool source_range_adds_exactly() {
    Session s;
    if (!load_clip(s))
        return false;
    s.openSource(0);
    s.seekSource(0.2); // frame 6
    s.markIn();
    s.stepFrames(9); // frame 15
    s.markOut();     // [6, 16)
    const double before = s.position();
    s.appendSelected();
    const bool exact = s.clips().size() == 2 && std::abs(clip_seconds(s, 1) - 10.0 / 30.0) < 1e-9 &&
                       s.position() >= before;
    s.undo();
    const bool undone = s.clips().size() == 1;
    const bool shown = std::abs(s.media().value(0).toMap().value("markIn").toDouble() - 0.2) < 1e-9;
    return exact && undone && shown;
}

// Marks that leave nothing (out at or before in) never produce an empty clip.
bool reversed_marks_reset() {
    Session s;
    if (!load_clip(s))
        return false;
    s.openSource(0);
    s.seekSource(0.5);
    s.markOut();       // out after frame 15
    s.seekSource(0.6); // frame 18: past out
    s.markIn();        // in past out: out is dropped, the range runs to the end
    const QVariantMap src = s.source();
    return src.value("out").toDouble() < 0 && std::abs(src.value("in").toDouble() - 0.6) < 1e-9;
}

// A sequence seek (timeline, menus) leaves the source viewer; source seeks do not move the
// sequence playhead.
bool playheads_stay_apart() {
    Session s;
    if (!load_clip(s))
        return false;
    s.seek(0.1);
    const double sequence = s.position();
    s.openSource(0);
    s.seekSource(0.8);
    const bool apart =
        s.position() == sequence && std::abs(s.source().value("position").toDouble() - 0.8) < 1e-9;
    s.seek(0.3);
    return apart && !s.source().value("open").toBool();
}

// Ripple preview: simulated with the delete command itself, changes nothing.
bool ripple_preview_names_clips() {
    Session s;
    if (!load_clip(s))
        return false;
    s.selectMedia(0);
    s.appendSelected(); // a second clip after the first
    if (s.clips().size() != 2)
        return false;
    const QVariant first = s.clips().at(0).toMap().value("id");
    const QString second = s.clips().at(1).toMap().value("id").toString();
    s.selectClip(first.toDouble());
    const QString undo = s.undoText();
    s.previewDelete(true);
    const QVariantMap scope = s.editScope();
    const bool named = scope.value("removed").toList().contains(first) &&
                       scope.value("moved").toMap().value(second).toInt() == -1;
    const bool untouched = s.clips().size() == 2 && s.undoText() == undo;
    s.previewDelete(false); // lift: a gap stays, nothing moves
    const bool lift = s.editScope().value("moved").toMap().isEmpty();
    s.clearEditScope();
    return named && untouched && lift && !s.editScope().value("active").toBool();
}

} // namespace

void run_session_tests() {
    describe("Session (headless)", {
        it("adds exactly the marked source range",
           { expect(source_range_adds_exactly()).toBeTruthy(); });
        it("drops an out mark that an in mark passes",
           { expect(reversed_marks_reset()).toBeTruthy(); });
        it("keeps the source and sequence playheads apart",
           { expect(playheads_stay_apart()).toBeTruthy(); });
        it("previews a delete with the delete command itself",
           { expect(ripple_preview_names_clips()).toBeTruthy(); });
    });
}
