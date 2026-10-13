#pragma once

// Video frames of `movie` counted by ffprobe's own decode (independent of OmaMovie), or -1.
#include <QProcess>
#include <QString>

inline int decoded_frames(const QString& movie) {
    QProcess probe;
    probe.start("ffprobe", {"-v", "error", "-select_streams", "v:0", "-count_frames",
                            "-show_entries", "stream=nb_read_frames", "-of", "default=nw=1:nk=1",
                            movie});
    return probe.waitForFinished(30000) && probe.exitCode() == 0
               ? probe.readAllStandardOutput().trimmed().toInt()
               : -1;
}
