#include "from_ir_dissolve.hpp"

#include "oma/timeline/edit.hpp"

namespace oma::project::from_ir_detail {

namespace tl = oma::timeline;

namespace {

struct Planned {
    const ir::Clip* clip;
    std::size_t edit;                // its set_transition in the edit list
    std::optional<std::size_t> roll; // the cut's centring roll just before it, if any
};

} // namespace

void place_dissolves(tl::Editor& ed, std::span<const std::pair<const ir::Clip*, tl::ClipId>> placed,
                     std::span<const std::uint8_t> landed,
                     std::span<const ir::Transition* const> into, ir::ImportReport& r) {
    std::vector<Edit> edits;
    std::vector<Planned> planned;
    for (std::size_t i = 0; i < placed.size(); ++i) {
        const auto [clip, id] = placed[i];
        if (landed[i] == 0)
            issue(r, clip->name, "clip cannot be placed (no media, or outside it)",
                  "left as a gap of its length");
        else if (into[i] != nullptr)
            if (const auto t = dissolve_of(ed.timeline(), *into[i], clip->name, r)) {
                Planned p{.clip = clip, .edit = 0, .roll = std::nullopt};
                if (auto roll = centring_roll(ed.timeline(), id, *into[i], clip->name, r)) {
                    p.roll = edits.size();
                    edits.push_back(std::move(*roll));
                }
                p.edit = edits.size();
                edits.emplace_back([id, t = *t] { return tl::edit::set_transition(id, t); });
                planned.push_back(p);
            }
    }
    std::vector<std::uint8_t> done(edits.size());
    execute_each(ed, edits, done);
    for (const Planned& p : planned)
        if (done[p.edit] == 0)
            issue(r, p.clip->name, "dissolve needs media the clips do not have",
                  "replaced by a hard cut");
        else if (p.roll && done[*p.roll] != 0)
            issue(r, p.clip->name, "dissolve is not centred on the cut",
                  "cut moved to its middle; the same frames mix");
        else if (p.roll)
            issue(r, p.clip->name, "dissolve is not centred on the cut",
                  "centred on the cut instead (no media to move it)");
}

} // namespace oma::project::from_ir_detail
