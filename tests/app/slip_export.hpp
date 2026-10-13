#pragma once

// The edit both export checks decode: `media` (1 s at 30 fps) split at frame 15, the second
// clip slipped three frames earlier, exported in software. Returns the movie, or empty.
#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>
#include <QTemporaryDir>

inline QString export_slipped(const QTemporaryDir& dir, const QString& media) {
    Session session;
    session.setSilent(true);
    session.open(QFileInfo(media).absoluteFilePath());
    if (!dir.isValid() ||
        !wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); }))
        return {};
    session.seek(0.5);
    session.splitAtPlayhead();
    session.selectClip(session.clips().at(1).toMap().value("id").toDouble());
    session.nudgeSlip(-3);
    const QString movie = dir.filePath("slip.mp4");
    session.setExportEncoder("software");
    session.exportMovie(QUrl::fromLocalFile(movie), "mp4-h264");
    const bool done = wait_for([&] { return session.exportProgress() < 0; }, 60000);
    return done && session.exportedFile() == movie ? movie : QString();
}
