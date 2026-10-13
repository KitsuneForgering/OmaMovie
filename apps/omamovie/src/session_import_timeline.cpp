// Importing an OpenTimelineIO file (M9, ADR-0008): read and convert on the job worker, probing
// each media file as the library import does, then open the result as an unsaved, untitled
// project and keep the import report for the UI (CLAUDE.md §16.4).
#include "session.hpp"
#include "timeline_import.hpp"

#include <QFileInfo>

QStringList Session::importReport() const {
    return import_report_;
}

void Session::importTimeline(const QUrl& url) {
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (path.isEmpty())
        return;
    setNotice(QStringLiteral("Importing %1…").arg(QFileInfo(path).fileName()));
    const unsigned generation = ++generation_;
    imports_.push_back(
        workers_.submit("import-timeline", [this, generation, path](oma::JobContext&) {
            QStringList issues;
            QString why;
            auto shared = std::make_shared<std::optional<oma::project::Document>>(
                import_timeline(path.toStdString(), *importer_, issues, why));
            QMetaObject::invokeMethod(
                this,
                [this, generation, path, issues, why, shared] {
                    if (generation != generation_)
                        return;
                    if (!*shared)
                        return fail(QStringLiteral("Cannot import %1: %2")
                                        .arg(QFileInfo(path).fileName(), why));
                    applyProject(std::move(**shared), QString());
                    saved_ = {}; // an import is unsaved work until the user saves it
                    import_report_ = issues;
                    emit importReportChanged();
                    emit projectChanged();
                    setNotice(issues.isEmpty()
                                  ? QStringLiteral("Imported %1").arg(QFileInfo(path).fileName())
                                  : QStringLiteral("Imported %1 with %2 notes in the import report")
                                        .arg(QFileInfo(path).fileName())
                                        .arg(issues.size()));
                },
                Qt::QueuedConnection);
            return oma::Result<void>{};
        }));
}
