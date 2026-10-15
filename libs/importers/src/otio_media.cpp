#include "otio_detail.hpp"

#include <format>

namespace oma::importers::otio {

namespace {
// Clip.2 keeps several references under a key; Clip.1 has one `media_reference`.
Result<Element> active_reference(Object clip) {
    Element ref;
    Object refs;
    if (clip["media_references"].get(refs) == simdjson::SUCCESS) {
        const std::string active = text(clip, "active_media_reference_key");
        const std::string key = active.empty() ? std::string("DEFAULT_MEDIA") : active;
        if (refs[key].get(ref) == simdjson::SUCCESS)
            return ref;
    } else if (clip["media_reference"].get(ref) == simdjson::SUCCESS) {
        return ref;
    }
    return make_error(ErrorCode::InvalidData, Category::Importer, "clip has no media reference",
                      text(clip, "name"));
}
} // namespace

Result<std::string> media_of(Object clip, ir::Timeline& tl, Context& c) {
    auto ref = active_reference(clip);
    if (!ref)
        return std::unexpected(ref.error());
    Object o;
    const std::string name = text(clip, "name");
    if (ref->get(o) != simdjson::SUCCESS)
        return make_error(ErrorCode::InvalidData, Category::Importer,
                          "media reference is not an object", name);
    const std::string schema = schema_of(o);
    const std::string target = text(o, "target_url");
    if (const auto same = c.media_by_target.find(target);
        !target.empty() && same != c.media_by_target.end())
        return same->second;
    ir::MediaRef media{.id = std::format("m{}", tl.media.size() + 1),
                       .target = target,
                       .available = std::nullopt,
                       .unsupported = {}};
    Element range;
    if (o["available_range"].get(range) == simdjson::SUCCESS && !range.is_null()) {
        auto r = range_of(range, c, name);
        if (!r)
            return std::unexpected(r.error());
        media.available = *r;
    }
    if (!schema.starts_with("ExternalReference.")) {
        media.unsupported.push_back(opaque(*ref, name));
        c.issue(name, std::format("media reference {} has no file", schema),
                "the clip is offline until relinked");
    }
    c.media_by_target.emplace(target, media.id); // an empty target is never looked up
    tl.media.push_back(std::move(media));
    return tl.media.back().id;
}

} // namespace oma::importers::otio
