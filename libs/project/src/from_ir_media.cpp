#include "from_ir_detail.hpp"

#include <string>

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

namespace {
std::size_t opaque_count(const ir::Timeline& timeline) {
    std::size_t n = timeline.unsupported.size();
    for (const ir::MediaRef& m : timeline.media)
        n += m.unsupported.size();
    for (const ir::Track& t : timeline.tracks) {
        n += t.unsupported.size();
        for (const ir::Item& item : t.items) {
            if (const auto* c = std::get_if<ir::Clip>(&item))
                n += c->unsupported.size();
            if (const auto* tr = std::get_if<ir::Transition>(&item))
                n += tr->unsupported.size();
        }
    }
    return n;
}
} // namespace

void report_unsaved(const ir::Timeline& timeline, ir::ImportReport& report) {
    // The native format has no place for opaque objects yet: say so rather than lose them silently.
    if (const std::size_t kept = opaque_count(timeline); kept > 0)
        issue(report, timeline.name,
              std::to_string(kept) + " source objects are not saved in the project",
              "re-import from the source file to recover them");
}

} // namespace oma::project::from_ir_detail
