#pragma once

// The edits the export checks decode: `media` (1 s at 30 fps) split at frame 15 with the second
// clip selected, then `edit`, exported in software. Returns the movie, or empty.
#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>
#include <QTemporaryDir>

#include <functional>

inline QString export_edit(const QTemporaryDir& dir, const QString& media,
                           const std::function<void(Session&)>& edit) {
    Session session;
    session.setSilent(true);
    session.open(QFileInfo(media).absoluteFilePath());
    if (!dir.isValid() ||
        !wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); }))
        return {};
    session.seek(0.5);
    session.splitAtPlayhead();
    session.selectClip(session.clips().at(1).toMap().value("id").toDouble());
    edit(session);
    const QString movie = dir.filePath("edit.mp4");
    session.setExportEncoder("software");
    session.exportMovie(QUrl::fromLocalFile(movie), "mp4-h264");
    const bool done = wait_for([&] { return session.exportProgress() < 0; }, 60000);
    return done && session.exportedFile() == movie ? movie : QString();
}

// Slipping the second clip three frames earlier.
inline QString export_slipped(const QTemporaryDir& dir, const QString& media) {
    return export_edit(dir, media, [](Session& s) { s.nudgeSlip(-3); });
}
