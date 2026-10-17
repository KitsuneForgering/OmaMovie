#include "autosave.hpp"
#include "session.hpp"
#include "wait_for.hpp"

#include "oma/project/document.hpp"

#include <QTemporaryDir>

#include "oma_test.hpp"

namespace {
bool preserves_unknown_fields_after_recovery() {
    // Earlier tests share the state folder; an autosave one of their Sessions left (slower runs
    // under sanitizers let its timer fire) would be offered too.
    for (const auto& left : autosave::list(autosave::dir()))
        autosave::remove(left.file);
    QTemporaryDir dir;
    if (!dir.isValid())
        return false;
    const QString path = dir.filePath("future.omamovie");
    oma::project::Document seed;
    auto timeline = oma::timeline::Timeline::create(oma::frame_rates::k30,
                                                    oma::SampleRate::make(48000).value());
    if (!timeline)
        return false;
    seed.timeline = std::move(*timeline);
    seed.unknown.emplace_back("future", R"({"nested":[1,"keep me"]})");
    if (!oma::project::save(seed, path.toStdString()))
        return false;
    std::filesystem::path copy;
    {
        Session session;
        session.openProject(QUrl::fromLocalFile(path));
        if (!wait_for([&] { return session.projectPath() == path; }))
            return false;
        session.setCanvasAspect(9, 16);
        if (!session.dirty())
            return false;
        session.autosave();
        if (!wait_for([&] {
                const auto found = autosave::list(autosave::dir());
                if (found.empty())
                    return false;
                copy = found.front().file;
                return oma::project::load(copy).has_value();
            }))
            return false;
    }
    Session restored;
    const auto offered = restored.recoveredProjects();
    if (offered.size() != 1 || offered.front().toMap().value("origin") != path)
        return false;
    restored.restoreRecovered(QString::fromStdString(copy.string()));
    if (!wait_for([&] { return restored.projectPath() == path && restored.dirty(); }))
        return false;
    restored.saveProject({});
    if (!wait_for([&] { return !restored.dirty(); }))
        return false;
    const auto saved = oma::project::load(path.toStdString());
    return saved && saved->unknown == seed.unknown && saved->canvas_width == 1080 &&
           saved->canvas_height == 1920;
}
} // namespace

void run_project_recovery_tests() {
    describe("Project recovery", {
        it("keeps unknown fields through autosave, recovery and manual save",
           { expect(preserves_unknown_fields_after_recovery()).toBeTruthy(); });
    });
}
