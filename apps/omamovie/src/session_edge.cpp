// The two-up display after a roll, slip or slide (edge_frames.hpp): both source frames are
// decoded on the job worker and published as images for the viewer overlay.
#include "edge_frames.hpp"
#include "session.hpp"
#include "ui_post.hpp"

#include <QUrl>

QVariantMap Session::edgeFrames() const {
    return edge_frames_;
}

void Session::showEdge(EdgeEdit edit, oma::timeline::ClipId clip) {
    if (!editor_)
        return;
    const auto sides = edge_sides(editor_->timeline(), clip, edit, ticksPerFrame(), *paths_);
    if (!sides)
        return;
    const unsigned request = ++edge_request_;
    const QString dir = thumbnails_.path();
    edge_job_.cancel(); // a newer nudge makes a queued request useless
    edge_job_ = workers_.submit("edge-frames", [this, request, sides = *sides,
                                                dir](oma::JobContext& job) {
        QVariantList urls;
        for (std::size_t i = 0; i < sides.size() && !job.is_cancelled(); ++i) {
            QString url;
            // Two files reused for every request: the query makes QML reload them.
            const QString file = QStringLiteral("%1/edge-%2.png").arg(dir).arg(i);
            if (!sides[i].path.empty()) {
                auto image = frames_.image_at(sides[i].path, sides[i].source);
                if (image && image->scaled(320, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                                 .save(file))
                    url = QUrl::fromLocalFile(file).toString() + QStringLiteral("?%1").arg(request);
            }
            urls.push_back(url);
        }
        post_to_ui(this, [this, request, urls] {
            if (request != edge_request_ || urls.size() != 2)
                return;
            edge_frames_ = {{"serial", request}, {"left", urls[0]}, {"right", urls[1]}};
            emit edgeFramesChanged();
        });
        return oma::Result<void>{};
    });
}
