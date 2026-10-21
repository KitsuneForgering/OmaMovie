#pragma once

#include "from_ir_detail.hpp"

namespace oma::project::from_ir_detail {

// The IR transition before a clip as a dissolve; none (a reported hard cut) for other kinds.
[[nodiscard]] std::optional<timeline::Transition> dissolve_of(const timeline::Timeline& timeline,
                                                              const ir::Transition& t,
                                                              const std::string& who,
                                                              ir::ImportReport& report);

// OmaMovie centres dissolves: an off-centre one (melt's mix lies before its cut, an EDL dissolve
// after it) rolls the cut into `into` to its middle, so the same frames mix. None if centred.
[[nodiscard]] std::optional<Edit> centring_roll(const timeline::Timeline& timeline,
                                                timeline::ClipId into, const ir::Transition& t,
                                                const std::string& who, ir::ImportReport& report);

// The dissolves into the placed clips (`into[i]` before `placed[i]`, if any), after reporting the
// clips that did not land; a dissolve that cannot be set becomes a reported hard cut.
void place_dissolves(timeline::Editor& editor,
                     std::span<const std::pair<const ir::Clip*, timeline::ClipId>> placed,
                     std::span<const std::uint8_t> landed,
                     std::span<const ir::Transition* const> into, ir::ImportReport& report);

} // namespace oma::project::from_ir_detail
