#include "recordings.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <system_error>

namespace omarchy {
namespace {

// A capture that changed this recently is treated as still being written.
constexpr auto kSettle = std::chrono::seconds(5);

bool is_video(const std::filesystem::path& p) {
    static constexpr std::array kExtensions{".mp4", ".mkv", ".webm", ".mov"};
    const std::string ext = p.extension().string();
    return std::ranges::any_of(kExtensions, [&](std::string_view e) { return ext == e; });
}

} // namespace
std::vector<Recording> list_recordings(const std::filesystem::path& dir, std::size_t limit) {
    std::vector<Recording> out;
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec || limit == 0)
        return out;
    const auto now = std::filesystem::file_time_type::clock::now();
    const std::filesystem::path current = current_recording();
    // Folders can hold thousands of captures: keep a heap of the newest `limit` (oldest at the
    // front), one stat per video file, O(files log limit).
    const auto newer = [](const Recording& a, const Recording& b) {
        return a.modified > b.modified;
    };
    for (const auto& entry : it) {
        if (!entry.is_regular_file(ec) || !is_video(entry.path()))
            continue;
        const auto modified = entry.last_write_time(ec);
        if (ec || (out.size() == limit && modified <= out.front().modified))
            continue;
        const bool recent = now - modified < kSettle && !current.empty();
        out.push_back(
            {.path = entry.path(),
             .modified = modified,
             .growing = recent && std::filesystem::equivalent(current, entry.path(), ec)});
        std::ranges::push_heap(out, newer);
        if (out.size() > limit) {
            std::ranges::pop_heap(out, newer);
            out.pop_back();
        }
    }
    std::ranges::sort_heap(out, newer);
    return out;
}

} // namespace omarchy
