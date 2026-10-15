#include "edl_detail.hpp"

#include <array>
#include <charconv>
#include <ranges>

namespace oma::importers::edl {

std::optional<std::int64_t> frames_of(std::string_view tc, std::int64_t fps, bool drop) {
    if (tc.size() != 11 || fps <= 0 || fps > 1000)
        return std::nullopt;
    std::array<std::int64_t, 4> part{};
    for (std::size_t i = 0; i < 4; ++i) {
        const char* first = tc.begin() + static_cast<std::ptrdiff_t>(i * 3);
        const auto [end, err] = std::from_chars(first, first + 2, part[i]);
        if (err != std::errc() || end != first + 2)
            return std::nullopt;
        const char sep = i < 3 ? tc[(i * 3) + 2] : ':';
        if (sep != ':' && (i != 2 || (sep != ';' && sep != '.')))
            return std::nullopt;
    }
    const auto [h, m, s, f] = part;
    if (m > 59 || s > 59 || f >= fps)
        return std::nullopt;
    const std::int64_t minutes = (h * 60) + m;
    std::int64_t frames = (((minutes * 60) + s) * fps) + f;
    if (drop) {
        // Two frame numbers (four at 59.94) are skipped each minute except every tenth.
        const std::int64_t skip = fps / 15;
        if (m % 10 != 0 && s == 0 && f < skip)
            return std::nullopt; // a number drop-frame never shows
        frames -= skip * (minutes - (minutes / 10));
    }
    return frames;
}

// The lexical pieces of a line: words and known prefixes.
std::vector<std::string_view> words(std::string_view line) {
    std::vector<std::string_view> out;
    for (auto w : line | std::views::split(' '))
        if (!w.empty())
            out.emplace_back(w.begin(), w.end());
    return out;
}

bool take(std::string_view& s, std::string_view prefix) {
    if (!s.starts_with(prefix))
        return false;
    s.remove_prefix(prefix.size());
    while (!s.empty() && s.front() == ' ')
        s.remove_prefix(1);
    return true;
}

} // namespace oma::importers::edl
