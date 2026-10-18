#pragma once

#include "oma/base/error.hpp"
#include "oma/project_ir/ir.hpp"

namespace oma::ir {

// Durations not negative, media ids unique, clips naming a known media id and, when the media's
// range is known, playing inside it. Importers run it before handing the IR on.
[[nodiscard]] Result<void> validate(const Timeline& timeline);

} // namespace oma::ir
