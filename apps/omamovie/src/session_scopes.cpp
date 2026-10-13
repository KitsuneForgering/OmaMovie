// Scopes (M9): the frame at the playhead rendered small on the CPU reference compositor (the
// same render graph the viewer draws, so grading and looks are measured), display-encoded and
// turned into a luma waveform and a vectorscope. On the job worker, on request (paused frame,
// scopes shown): it is not in the playback path, so the CPU render is no readback of the
// viewer (CLAUDE.md §7.2).
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
            return !display.isNull() && image.save(file)
                       ? QUrl::fromLocalFile(file).toString() + QStringLiteral("?%1").arg(request)
                       : QString();
        };
        const QString wave =
            display.isNull() ? QString() : save(luma_waveform(display), "waveform");
        const QString vector =
            display.isNull() ? QString() : save(vectorscope(display), "vectorscope");
        QMetaObject::invokeMethod(
            this,
            [this, request, wave, vector] {
                if (request != scope_request_)
                    return;
                scope_image_ = wave;
                vectorscope_image_ = vector;
                emit scopeImageChanged();
            },
            Qt::QueuedConnection);
        return oma::Result<void>{};
    });
}
