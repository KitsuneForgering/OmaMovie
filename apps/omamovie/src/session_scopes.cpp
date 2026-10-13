// Scopes (M9): a luma waveform, an RGB parade and a vectorscope of the frame at the playhead, from
// a small CPU render of the viewer's graph (scope_frame.hpp). On the job worker, on request only:
// not in the playback path, so it is no readback of the viewer (CLAUDE.md §7.2).
#include "scope_frame.hpp"
#include "scopes.hpp"
#include "session.hpp"

#include <QUrl>

QString Session::scopeImage() const {
    return scope_image_;
}
QString Session::vectorscopeImage() const {
    return vectorscope_image_;
}

QString Session::paradeImage() const {
    return parade_image_;
}

void Session::requestScopes() {
    if (!editor_ || !hasMedia())
        return;
    const unsigned request = ++scope_request_;
    const QString dir = thumbnails_.path();
    scope_job_.cancel(); // a newer frame makes a queued request useless
    scope_job_ = workers_.submit("scopes", [this, request, dir, timeline = editor_->timeline(),
                                            paths = paths_, luts = luts_, frame = frame(),
                                            tpf = ticksPerFrame()](oma::JobContext&) {
        const QImage display = small_frame(timeline, *paths, *luts, frame, tpf, frames_);
        // Two files per scope, reused: the query makes QML reload them.
        const auto save = [&](const QImage& image, const char* name) {
            const QString file =
                QStringLiteral("%1/%2-%3.png").arg(dir, QLatin1String(name)).arg(request % 2);
            return image.save(file)
                       ? QUrl::fromLocalFile(file).toString() + QStringLiteral("?%1").arg(request)
                       : QString();
        };
        QString wave;
        QString vector;
        QString parade;
        if (!display.isNull()) {
            wave = save(luma_waveform(display), "waveform");
            vector = save(vectorscope(display), "vectorscope");
            parade = save(rgb_parade(display), "parade");
        }
        QMetaObject::invokeMethod(
            this,
            [this, request, wave, vector, parade] {
                if (request != scope_request_)
                    return;
                scope_image_ = wave;
                vectorscope_image_ = vector;
                parade_image_ = parade;
                emit scopeImageChanged();
            },
            Qt::QueuedConnection);
        return oma::Result<void>{};
    });
}
