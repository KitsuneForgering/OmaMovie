#pragma once

// Connected clips (`lane`) of the FCPXML spine as further IR tracks.

#include "fcpxml_detail.hpp"

#include <map>
#include <vector>

namespace oma::importers::fcpxml {

// A clip at a timeline position, before its lane is laid out (connected clips).
struct Placed {
    RationalTime at;
    ir::Clip clip;
};
// The connected children (`lane` set) of spine item `e`, placed at timeline `offset`: clips into
// `lanes`, anything else kept opaque and reported.
void connect(Context& c, const xml::Element& e, const RationalTime& offset,
             std::map<int, std::vector<Placed>>& lanes);
// Appends `items` to `track` in timeline order, with gaps where it is empty.
void lay_out(Context& c, std::vector<Placed>& items, ir::Track& track);

} // namespace oma::importers::fcpxml
