#include "oma/project_ir/validate.hpp"

#include <format>
#include <unordered_map>

namespace oma::ir {

namespace {
std::unexpected<Error> invalid(std::string message, std::string context) {
    return make_error(ErrorCode::InvalidData, Category::Importer, std::move(message),
                      std::move(context));
}

bool negative(const RationalTime& t) {
    return t < RationalTime{};
}
} // namespace

Result<void> validate(const Timeline& timeline) {
    std::unordered_map<std::string, const MediaRef*> media;
    for (const MediaRef& m : timeline.media) {
        if (m.id.empty() || !media.emplace(m.id, &m).second)
            return invalid("media id is empty or repeated", m.id);
    }
    for (const Track& track : timeline.tracks) {
        for (std::size_t i = 0; i < track.items.size(); ++i) {
            const std::string where = std::format("{} item {}", track.name, i);
            const Item& item = track.items[i];
            if (const auto* gap = std::get_if<Gap>(&item); gap && negative(gap->duration))
                return invalid("negative gap", where);
            if (const auto* t = std::get_if<Transition>(&item);
                t && (negative(t->in_offset) || negative(t->out_offset)))
                return invalid("negative transition offset", where);
            const auto* clip = std::get_if<Clip>(&item);
            if (clip == nullptr)
                continue;
            const auto found = media.find(clip->media);
            if (found == media.end())
                return invalid("clip refers to unknown media", where + " " + clip->media);
            if (negative(clip->source.duration()))
                return invalid("negative clip duration", where);
            const auto& available = found->second->available;
            if (available && (clip->source.start() < available->start() ||
                              available->end() < clip->source.end()))
                return invalid("clip plays outside its media", where);
        }
    }
    return {};
}

} // namespace oma::ir
