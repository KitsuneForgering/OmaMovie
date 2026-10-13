#pragma once

#include "oma/timeline/model.hpp"

#include <QVariantMap>

// What an edit does to the clips and markers of `before`, for the timeline's preview marks:
// {active, moved: {id: -1 | 1}, removed: [id], markers: count}.
[[nodiscard]] QVariantMap edit_scope(const oma::timeline::Timeline& before,
                                     const oma::timeline::Timeline& after);
