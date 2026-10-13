#include "from_ir_detail.hpp"

namespace oma::project::from_ir_detail {

namespace tl = oma::timeline;

void resolve_media(tl::Editor& editor, const ir::Timeline& timeline, const ResolveMedia& resolve,
                   Document& doc, MediaIds& ids, ir::ImportReport& report) {
    for (const ir::MediaRef& m : timeline.media) {
        std::optional<MediaRef> found = resolve(m);
        if (found) {
            found->info.id = tl::MediaId(static_cast<std::uint64_t>(doc.media.size() + 1));
            if (editor.add_media(found->info)) {
                ids.emplace(m.id, found->info.id);
                doc.media.push_back(std::move(*found));
                continue;
            }
        }
        issue(report, m.target, "media file not found or not readable", "its clips become gaps");
    }
}

} // namespace oma::project::from_ir_detail
