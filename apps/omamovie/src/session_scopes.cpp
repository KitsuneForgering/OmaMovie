// Scopes (M9): the frame at the playhead rendered small on the CPU reference compositor (the
// same render graph the viewer draws, so grading and looks are measured), display-encoded and
// turned into a luma waveform. On the job worker, on request (paused frame, scopes shown): it is
// not in the playback path, so the CPU render is no readback of the viewer (CLAUDE.md §7.2).
#include "scopes.hpp"
#include "session.hpp"
#include "viewer_frame.hpp"

#include <QUrl>

#include <vector>

QString Session::scopeImage() const {
    return scope_image_;
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
        QString url;
        auto built = build_viewer_frame(timeline, *paths, *luts, 256, 144, frame, tpf, frames_);
        if (built) {
            std::vector<oma::compositor::LayerInput> inputs;
            for (const Picture& p : (*built)->pictures)
                inputs.push_back({.frame = p.frame.get(),
                                  .color = p.color,
                                  .rotation = p.rotation,
                                  .sample_aspect = p.sample_aspect});
            auto rendered = oma::compositor::CpuCompositor{}.render((*built)->graph, inputs);
            const QString file = QStringLiteral("%1/scope-%2.png").arg(dir).arg(request % 2);
            if (rendered && luma_waveform(to_srgb_image(*rendered)).save(file))
                url = QUrl::fromLocalFile(file).toString() + QStringLiteral("?%1").arg(request);
        }
        QMetaObject::invokeMethod(
            this,
            [this, request, url] {
                if (request != scope_request_)
                    return;
                scope_image_ = url;
                emit scopeImageChanged();
            },
            Qt::QueuedConnection);
        return oma::Result<void>{};
    });
}
