#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>

#include <cmath>

#include "oma_test.hpp"

namespace {
QUrl fixture(const char* name) {
    return QUrl::fromLocalFile(
        QFileInfo(QString("tests/importers/fixtures/") + name).absoluteFilePath());
}

// relative.otio places half a second of the generated camera clip, found next to it.
bool imports_an_otio_file_as_an_unsaved_project() {
    Session s;
    s.setSilent(true);
    s.importTimeline(fixture("relative.otio"));
    if (!wait_for([&] { return s.clips().size() == 1 && !s.importsPending(); }))
        return false;
    const double length = s.clips().front().toMap().value("duration").toDouble();
    // The canvas is the clip's own 320x180, not the 1920x1080 default.
    return std::abs(length - 0.5) < 1e-9 && s.projectPath().isEmpty() && s.dirty() &&
           s.canvasWidth() == 320 && s.canvasHeight() == 180 && s.importReport().isEmpty();
}

// relative.mlt is the same half second as an MLT (melt/Kdenlive) project.
bool imports_an_mlt_file() {
    Session s;
    s.setSilent(true);
    s.importTimeline(fixture("relative.mlt"));
    if (!wait_for([&] { return s.clips().size() == 1 && !s.importsPending(); }))
        return false;
    return std::abs(s.clips().front().toMap().value("duration").toDouble() - 0.5) < 1e-9 &&
           s.canvasWidth() == 320;
}

// speeds.mlt: a Kdenlive speed with sound the mixer can follow (2x) and one it cannot (0.7x).
bool notes_a_speed_that_plays_silent() {
    Session s;
    s.setSilent(true);
    s.importTimeline(fixture("speeds.mlt"));
    if (!wait_for([&] { return s.clips().size() == 2 && !s.importsPending(); }))
        return false;
    const QVariantList clips = s.clips();
    return std::abs(clips[0].toMap().value("duration").toDouble() - (5.0 / 30)) < 1e-9 &&
           std::abs(clips[1].toMap().value("duration").toDouble() - (10.0 / 30)) < 1e-9 &&
           s.importReport() == QStringList{"h264_30fps_aac.mp4: speed 7/10 plays without sound; "
                                           "the picture keeps it"};
}

// basic.otio refers to a file that does not exist: it opens with notes instead of clips.
bool reports_what_an_import_could_not_place() {
    Session s;
    s.setSilent(true);
    s.importTimeline(fixture("basic.otio"));
    if (!wait_for([&] { return !s.importReport().isEmpty(); }))
        return false;
    return s.clips().isEmpty() && s.importReport().filter("media file not found").size() == 1;
}
} // namespace

void run_import_timeline_tests() {
    describe("Timeline import", {
        it("imports an OTIO file as an unsaved project",
           { expect(imports_an_otio_file_as_an_unsaved_project()).toBeTruthy(); });
        it("imports an MLT file the same way", { expect(imports_an_mlt_file()).toBeTruthy(); });
        it("notes an imported speed that plays without sound",
           { expect(notes_a_speed_that_plays_silent()).toBeTruthy(); });
        it("reports what an import could not place",
           { expect(reports_what_an_import_could_not_place()).toBeTruthy(); });
    });
}
