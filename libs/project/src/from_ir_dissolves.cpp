#include "from_ir_detail.hpp"

#include "oma/timeline/edit.hpp"

namespace oma::project::from_ir_detail {

namespace tl = oma::timeline;

void place_dissolves(tl::Editor& ed, std::span<const std::pair<const ir::Clip*, tl::ClipId>> placed,
                     std::span<const std::uint8_t> landed,
                     std::span<const ir::Transition* const> into, ir::ImportReport& r) {
    std::vector<Edit> dissolves;
    std::vector<const ir::Clip*> dissolved;
    for (std::size_t i = 0; i < placed.size(); ++i) {
        const auto [clip, id] = placed[i];
        if (landed[i] == 0)
            issue(r, clip->name, "clip cannot be placed (no media, or outside it)",
                  "left as a gap of its length");
        else if (into[i] != nullptr)
            if (const auto t = dissolve_of(ed.timeline(), *into[i], clip->name, r)) {
                dissolves.emplace_back([id, t = *t] { return tl::edit::set_transition(id, t); });
                dissolved.push_back(clip);
            }
    }
    std::vector<std::uint8_t> mixed(dissolves.size());
    execute_each(ed, dissolves, mixed);
    for (std::size_t i = 0; i < dissolved.size(); ++i)
        if (mixed[i] == 0)
            issue(r, dissolved[i]->name, "dissolve needs media the clips do not have",
                  "replaced by a hard cut");
}

} // namespace oma::project::from_ir_detail
