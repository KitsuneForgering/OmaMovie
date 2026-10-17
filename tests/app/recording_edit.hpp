#pragma once

// The recording workflow (M10): a recording-shaped file (screenrecording_like.mp4, 60 fps with
// 96 kHz sound) is split at frame 15, its head ripple-deleted, the project saved and reopened,
// and the result exported in software. Returns the movie, or empty.
#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>
#include <QTemporaryDir>

inline QString export_recording_edit(const QTemporaryDir& dir, const QString& recording,
                                     int export_ms = 60000) {
    Session session;
    session.setSilent(true);
    session.open(QFileInfo(recording).absoluteFilePath());
    if (!wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); }))
        return {};
    session.seek(0.25);
    session.splitAtPlayhead();
    session.selectClipAt(0.1);
    session.deleteSelected(true);
    const QString project = dir.filePath("recording.omamovie");
    session.saveProject(QUrl::fromLocalFile(project));
    if (session.clips().size() != 1 ||
        !wait_for([&] { return session.projectPath() == project && !session.dirty(); }))
        return {};
    Session reopened;
    reopened.setSilent(true);
    reopened.openProject(QUrl::fromLocalFile(project));
    if (!wait_for([&] { return reopened.clips().size() == 1 && !reopened.importsPending(); }))
        return {};
    const QString movie = dir.filePath("recording.mp4");
    reopened.setExportEncoder("software");
    reopened.exportMovie(QUrl::fromLocalFile(movie), "mp4-h264");
    const bool done = wait_for([&] { return reopened.exportProgress() < 0; }, export_ms);
    return done && reopened.exportedFile() == movie ? movie : QString();
}
