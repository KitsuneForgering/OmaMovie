// Library thumbnails follow a media item's color interpretation (ADR-0019): after an edit (the
// Info drawer's choice, or its undo) the items whose reading changed are redrawn on the worker.
#include "session.hpp"
#include "thumbnail.hpp"
#include "ui_post.hpp"

#include <QUrl>

namespace tl = oma::timeline;

namespace {
bool same_reading(const tl::MediaInfo& a, const tl::MediaInfo& b) {
    return a.color_matrix == b.color_matrix && a.color_range == b.color_range;
}
} // namespace

void Session::refreshThumbnails() {
    if (!editor_)
        return;
    struct Redraw {
        tl::MediaInfo media; // with the timeline's reading
        QString path;
        std::string identity;
        QString out;
    };
    std::vector<Redraw> redraws;
    for (const LibraryItem& item : library_) {
        const tl::MediaInfo* m = editor_->timeline().find_media(item.media.id);
        if (item.audio_only || item.missing || m == nullptr || same_reading(*m, item.media))
            continue;
        tl::MediaInfo media = item.media;
        media.color_matrix = m->color_matrix;
        media.color_range = m->color_range;
        redraws.push_back({.media = media,
                           .path = item.path,
                           .identity = media_identity(item.path, item.fingerprint),
                           .out = thumbnails_.filePath(QString::number(item.media.id.value()) +
                                                       QStringLiteral(".png"))});
    }
    if (redraws.empty())
        return;
    // A newer edit recomputes the whole list from the items still out of date.
    thumbnail_job_.cancel();
    const unsigned generation = generation_;
    const unsigned serial = ++thumbnail_serial_;
    thumbnail_job_ = workers_.submit("thumbnails", [this, redraws = std::move(redraws), generation,
                                                    serial](oma::JobContext& job) {
        for (const Redraw& r : redraws) {
            if (job.is_cancelled())
                break;
            const bool drawn = write_thumbnail(frames_, thumbnail_cache_, r.path.toStdString(),
                                               r.identity, r.media, r.out);
            post_to_ui(this, [this, r, drawn, generation, serial] {
                if (generation != generation_)
                    return;
                for (LibraryItem& item : library_) {
                    if (item.media.id != r.media.id)
                        continue;
                    item.media.color_matrix = r.media.color_matrix;
                    item.media.color_range = r.media.color_range;
                    // Same file, new picture: the query makes QML load it again.
                    if (drawn)
                        item.thumbnail = QUrl::fromLocalFile(r.out).toString() +
                                         QStringLiteral("?%1").arg(serial);
                }
                emit libraryChanged();
            });
        }
        return oma::Result<void>{};
    });
}
