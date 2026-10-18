// A media item's color interpretation (ADR-0019). The timeline holds it, so undo, the renderers
// and saving all see one value; library items are copied from it when the project is written.
#include "session.hpp"

#include "oma/timeline/edit.hpp"

namespace tl = oma::timeline;

oma::project::MediaRef Session::mediaRef(const LibraryItem& item) const {
    oma::project::MediaRef ref{.info = item.media,
                               .path = item.path.toStdString(),
                               .name = item.name.toStdString(),
                               .audio_only = item.audio_only,
                               .fingerprint = item.fingerprint};
    if (const tl::MediaInfo* m =
            editor_ ? editor_->timeline().find_media(item.media.id) : nullptr) {
        ref.info.color_matrix = m->color_matrix;
        ref.info.color_range = m->color_range;
    }
    return ref;
}

void Session::setMediaColor(int matrix, int range) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr || c->title || matrix < 0 || matrix > 3 || range < 0 || range > 2)
        return;
    run(tl::edit::set_media_color(c->media, static_cast<tl::MediaColorMatrix>(matrix),
                                  static_cast<tl::MediaColorRange>(range)));
}
