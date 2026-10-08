#include "session.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUrl>

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

// Captions (ADR-0017): added at the playhead up to the next one, never over another; imported
// from SRT onto the frame grid; exported again as SRT that reads the same.
bool captions_round_trip() {
    Session s;
    if (!load_clip(s))
        return false;
    QTemporaryDir dir;
    QFile srt(dir.filePath("in.srt"));
    if (!srt.open(QIODevice::WriteOnly))
        return false;
    srt.write(
        "1\n00:00:00,100 --> 00:00:00,450\nHello\n\n2\n00:00:00,500 --> 00:00:00,900\nWorld\n");
    srt.close();
    s.importCaptions(QUrl::fromLocalFile(srt.fileName()));
    const QVariantList c = s.captions();
    // 0.1 s → frame 3 (floor); 0.45 s → frame 14 (ceil), cut at the next start (frame 15).
    const bool imported =
        c.size() == 2 && std::abs(c[0].toMap().value("start").toDouble() - 0.1) < 1e-9 &&
        std::abs(c[0].toMap().value("duration").toDouble() - 11.0 / 30.0) < 1e-9 &&
        s.captionAt(0.2) == QStringLiteral("Hello") && s.captionAt(0.95).isEmpty();
    s.seek(0.2);
    const bool refused = s.addCaption() == 0; // a caption is already there
    s.seek(0.0);
    const double id = s.addCaption(); // [0, 0.1): up to the next caption
    const bool added =
        id > 0 && std::abs(s.captions().front().toMap().value("duration").toDouble() - 0.1) < 1e-9;
    s.setCaptionText(id, QStringLiteral("Olá"));
    s.exportCaptions(QUrl::fromLocalFile(dir.filePath("out.srt")));
    QFile out(dir.filePath("out.srt"));
    const bool written = out.open(QIODevice::ReadOnly) &&
                         out.readAll().startsWith("1\n00:00:00,000 --> 00:00:00,100\nOlá");
    s.undo(); // the text edit
    return imported && refused && added && written &&
           s.captionAt(0.05) == QStringLiteral("Caption");
}

// Canvas proportion (ADR-0010): 9:16 keeps the short side, follows undo, and survives a save.
bool canvas_aspect_changes() {
    Session s;
    if (!load_clip(s))
        return false; // 320x180 fixture
    const bool start = s.canvasWidth() == 320 && s.canvasHeight() == 180;
    s.setCanvasAspect(9, 16);
    const bool vertical = s.canvasWidth() == 180 && s.canvasHeight() == 320;
    s.undo();
    const bool back = s.canvasWidth() == 320 && s.canvasHeight() == 180;
    s.redo();
    return start && vertical && back && s.canvasWidth() == 180;
}

// Named speed ramps (M8): accelerate keeps the duration, burst takes two thirds; undo restores.
bool speed_ramps() {
    Session s;
    if (!load_clip(s))
        return false; // 30 frames
    s.selectClip(s.clips().front().toMap().value("id").toDouble());
    s.setSpeedRamp(0);
    const QVariantMap up = s.clips().front().toMap();
    const bool same = std::abs(up.value("duration").toDouble() - 1.0) < 1e-9 &&
                      up.value("timing").toString() == "Ramp";
    s.undo();
    s.setSpeedRamp(2);
    const bool burst = std::abs(clip_seconds(s, 0) - 20.0 / 30.0) < 1e-9;
    s.undo();
    return same && burst && std::abs(clip_seconds(s, 0) - 1.0) < 1e-9 &&
           s.clips().front().toMap().value("timing").toString().isEmpty();
}

// Opacity and volume keys (M8): a toggle keeps the value shown, a slider then sets the key at
// the playhead, a fade change adds no key, and removing the last key leaves its value.
bool scalar_keys() {
    Session s;
    if (!load_clip(s))
        return false;
    s.selectClip(s.clips().front().toMap().value("id").toDouble());
    s.seek(0.0);
    s.toggleOpacityKey();
    s.toggleVolumeKey();
    s.seek(0.5);
    s.setClipOpacity(0.2);
    s.setClipAudio(0.5, 0.0, 0.0, false);
    s.setClipAudio(0.5, 0.1, 0.0, false); // only the fade
    const QVariantMap here = s.motion();
    const bool keyed = here.value("opacityKeys").toInt() == 2 &&
                       here.value("gainKeys").toInt() == 2 &&
                       here.value("opacityKeyHere").toBool() &&
                       std::abs(here.value("gain").toDouble() - 0.5) < 1e-6;
    s.seek(0.2);
    const double between = s.motion().value("opacity").toDouble();
    const bool interpolated =
        between > 0.2 && between < 1.0 && !s.motion().value("opacityKeyHere").toBool();
    s.seek(0.5);
    s.toggleVolumeKey(); // removes the 0.5 s key
    s.seek(0.0);
    s.toggleVolumeKey(); // the last one: its 1.0 stays as the volume
    const QVariantMap after = s.motion();
    return keyed && interpolated && after.value("gainKeys").toInt() == 0 &&
           std::abs(after.value("gain").toDouble() - 1.0) < 1e-6;
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
        it("applies named speed ramps with undo", { expect(speed_ramps()).toBeTruthy(); });
        it("changes the canvas proportion with undo",
           { expect(canvas_aspect_changes()).toBeTruthy(); });
        it("adds, imports and exports captions on the frame grid",
           { expect(captions_round_trip()).toBeTruthy(); });
        it("keys opacity and volume at the playhead", { expect(scalar_keys()).toBeTruthy(); });
    });
}
