#pragma once

// Imports a timeline file, saves it as a project, reopens that project in a fresh Session and
// exports it with `encoder` (auto, hardware or software) into `dir`. Returns the movie, or empty
// if any step failed; `route` receives the encoder actually used and any fallback reason.
#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>
#include <QTemporaryDir>

#include <string>

inline QString export_reopened_import(const QTemporaryDir& dir, const QString& timeline,
                                      qsizetype clips, const QString& encoder = "software",
                                      std::string* route = nullptr) {
    Session imported;
    imported.setSilent(true);
    imported.importTimeline(QUrl::fromLocalFile(QFileInfo(timeline).absoluteFilePath()));
    if (!dir.isValid() ||
        !wait_for([&] { return imported.clips().size() == clips && !imported.importsPending(); }))
        return {};
    const QString project = dir.filePath("reopened.omamovie");
    imported.saveProject(QUrl::fromLocalFile(project));
    if (!wait_for([&] { return imported.projectPath() == project && !imported.dirty(); }))
        return {};
    Session reopened;
    reopened.setSilent(true);
    reopened.openProject(QUrl::fromLocalFile(project));
    if (!wait_for([&] { return reopened.clips().size() == clips && !reopened.importsPending(); }))
        return {};
    const QString movie = dir.filePath("reopened.mp4");
    reopened.setExportEncoder(encoder);
    reopened.exportMovie(QUrl::fromLocalFile(movie), "mp4-h264");
    const bool done = wait_for([&] { return reopened.exportProgress() < 0; }, 60000);
    if (route != nullptr)
        *route = reopened.lastExportStats().encoder;
    return done && reopened.exportedFile() == movie ? movie : QString();
}
