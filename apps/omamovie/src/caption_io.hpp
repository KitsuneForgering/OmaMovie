#pragma once

#include "oma/project/subtitles.hpp"
#include "oma/timeline/model.hpp"

#include <QString>

#include <span>
#include <vector>

namespace caption_io {
[[nodiscard]] std::vector<oma::project::Cue> cues(const oma::timeline::Timeline& timeline);
[[nodiscard]] bool write(const QString& path, std::span<const oma::project::Cue> cues);
} // namespace caption_io
