// M10 failure and recovery: an autosave that cannot write (a read-only state folder) leaves the
// project untouched and is retried, not marked done; the manual save still works.
#include "autosave.hpp"
#include "session.hpp"
#include "wait_for.hpp"

#include "oma/project/document.hpp"

#include <QTemporaryDir>
#include <unistd.h>

#include "oma_test.hpp"

namespace {
namespace fs = std::filesystem;

bool autosaved() {
    const auto found = autosave::list(autosave::dir());
    return !found.empty() && oma::project::load(found.front().file);
}

bool failed_autosave_is_retried() {
    for (const auto& left : autosave::list(autosave::dir()))
        autosave::remove(left.file);
    QTemporaryDir dir;
    const QString path = dir.filePath("project.omamovie");
    oma::project::Document seed;
    seed.timeline =
        oma::timeline::Timeline::create(oma::frame_rates::k30, oma::SampleRate::make(48000).value())
            .value();
    if (!dir.isValid() || !oma::project::save(seed, path.toStdString()))
        return false;
    Session session;
    session.openProject(QUrl::fromLocalFile(path));
    if (!wait_for([&] { return session.projectPath() == path; }))
        return false;
    session.setCanvasAspect(9, 16);
    fs::create_directories(autosave::dir());
    fs::permissions(autosave::dir(), fs::perms::owner_read | fs::perms::owner_exec);
    session.autosave();
    const bool refused = !wait_for([] { return autosaved(); }, 1500); // nothing may appear
    fs::permissions(autosave::dir(), fs::perms::owner_all);
    session.autosave(); // the failure was not taken for a success: this one writes
    const bool retried = wait_for([] { return autosaved(); });
    session.saveProject({});
    const bool saved = wait_for([&] { return !session.dirty(); });
    const auto on_disk = oma::project::load(path.toStdString());
    return refused && retried && saved && on_disk && on_disk->canvas_width == 1080;
}

} // namespace

void run_autosave_failure_tests() {
    describe("Autosave failure", {
        it("leaves the project alone and retries after a refused write", {
            if (::geteuid() != 0) // root writes anyway
                expect(failed_autosave_is_retried()).toBeTruthy();
        });
    });
}
