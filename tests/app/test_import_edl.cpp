// An EDL through the app's import (M9): the dialog asks for the frame rate its timecodes count in
// (EDLs name none) and passes it on; an unreadable rate imports nothing.
#include "session.hpp"
#include "wait_for.hpp"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <cmath>

#include "oma_test.hpp"

namespace {

// Two cuts of the camera fixture at 25 fps: 10 frames (0.4 s) then 5 (0.2 s).
QString write_edl(const QTemporaryDir& dir) {
    const QString media =
        QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath();
    QFile file(dir.filePath("cuts.edl"));
    if (!file.open(QIODevice::WriteOnly))
        return {};
    file.write(QStringLiteral("001 AX V C 00:00:00:05 00:00:00:15 01:00:00:00 01:00:00:10\n"
                              "* SOURCE FILE: %1\n"
                              "002 AX V C 00:00:00:00 00:00:00:05 01:00:00:10 01:00:00:15\n"
                              "* SOURCE FILE: %1\n")
                   .arg(media)
                   .toUtf8());
    return file.fileName();
}

bool imports_at_the_chosen_rate() {
    QTemporaryDir dir;
    const QString edl = write_edl(dir);
    Session s;
    s.setSilent(true);
    s.importTimeline(QUrl::fromLocalFile(edl), QStringLiteral("not a rate"));
    QCoreApplication::processEvents();
    if (!s.clips().isEmpty() || s.importsPending())
        return false; // refused before any work
    s.importTimeline(QUrl::fromLocalFile(edl), QStringLiteral("25"));
    if (!wait_for([&] { return s.clips().size() == 2 && !s.importsPending(); }))
        return false;
    const auto first = s.clips().at(0).toMap().value("duration").toDouble();
    const auto second = s.clips().at(1).toMap().value("duration").toDouble();
    return std::abs(first - 0.4) < 1e-6 && std::abs(second - 0.2) < 1e-6;
}

} // namespace

void run_import_edl_tests() {
    describe("EDL import", {
        it("imports at the frame rate the user chose",
           { expect(imports_at_the_chosen_rate()).toBeTruthy(); });
    });
}
