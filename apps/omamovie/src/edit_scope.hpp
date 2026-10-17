#pragma once

#include "oma/timeline/model.hpp"

#include <QVariantMap>

// What an edit does to the clips and markers of `before`, for the timeline's preview marks:
// {active, moved: {id: -1 | 1}, removed: [id], cut: [id], markers: count}. A clip is cut when
// it stays but loses part of its range (an overwrite or a trim over it); a split clip keeps its
// ID for the left part.
[[nodiscard]] QVariantMap edit_scope(const oma::timeline::Timeline& before,
                                     const oma::timeline::Timeline& after);
