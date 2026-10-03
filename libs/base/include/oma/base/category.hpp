#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace oma {

// Subsystem that a log record or an error belongs to (CLAUDE.md §19).
enum class Category : std::uint8_t {
    Base,
    Media,
    Decode,
    Encode,
    Gpu,
    Compositor,
    Timeline,
    Project,
    Importer,
    Audio,
    Cache,
    Playback,
    Ui,
};

inline constexpr std::size_t kCategoryCount = static_cast<std::size_t>(Category::Ui) + 1;

[[nodiscard]] std::string_view to_string(Category category) noexcept;

} // namespace oma
