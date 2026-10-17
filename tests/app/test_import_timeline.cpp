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
        it("reports what an import could not place",
           { expect(reports_what_an_import_could_not_place()).toBeTruthy(); });
    });
}
