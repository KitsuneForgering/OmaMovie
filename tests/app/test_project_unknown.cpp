#include "oma/project/document.hpp"
#include "session.hpp"
#include "wait_for.hpp"

#include <QFile>
#include <QTemporaryDir>

#include "oma_test.hpp"

namespace {
bool preserves_unknown_fields_in_app_save() {
    QTemporaryDir dir;
    const QString input = dir.filePath("future.omamovie");
    QFile file(input);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(R"({"format":"omamovie-project","format_version":1,"future":{"a":[1,"x"]}})") <
            0)
        return false;
    file.close();
    Session session;
    session.openProject(QUrl::fromLocalFile(input));
    if (!wait_for([&] { return session.projectPath() == input; }))
        return false;
    const QString saved = dir.filePath("saved.omamovie");
    session.saveProject(QUrl::fromLocalFile(saved));
    if (!wait_for([&] { return session.projectPath() == saved; }))
        return false;
    const auto result = oma::project::load(saved.toStdString());
    if (!result || result->unknown.size() != 1 || result->unknown[0].first != "future")
        return false;
    session.newProject();
    const QString fresh = dir.filePath("fresh.omamovie");
    session.saveProject(QUrl::fromLocalFile(fresh));
    if (!wait_for([&] { return session.projectPath() == fresh; }))
        return false;
    const auto new_project = oma::project::load(fresh.toStdString());
    return new_project && new_project->unknown.empty();
}
} // namespace

void run_project_unknown_tests() {
    describe("Project fields through Session", {
        it("preserves compatible unknown fields and clears them for a new project",
           { expect(preserves_unknown_fields_in_app_save()).toBeTruthy(); });
    });
}
