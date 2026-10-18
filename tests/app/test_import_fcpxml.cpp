// A Final Cut Pro bundle through the app's import (M9): `relative.fcpxmld` is half a second of the
// camera fixture, its relative path resolved inside the bundle as Final Cut Pro writes them.
#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>

#include <cmath>

#include "oma_test.hpp"

namespace {

bool imports_an_fcpxml_bundle() {
    Session s;
    s.setSilent(true);
    s.importTimeline(QUrl::fromLocalFile(
        QFileInfo("tests/importers/fixtures/relative.fcpxmld").absoluteFilePath()));
    if (!wait_for([&] { return s.clips().size() == 1 && !s.importsPending(); }))
        return false;
    return std::abs(s.clips().front().toMap().value("duration").toDouble() - 0.5) < 1e-9 &&
           s.canvasWidth() == 320 && s.importReport().isEmpty();
}

} // namespace

void run_import_fcpxml_tests() {
    describe("FCPXML import", {
        it("imports a bundle with its media found inside it",
           { expect(imports_an_fcpxml_bundle()).toBeTruthy(); });
    });
}
