#include "recordings.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <string_view>
#include <system_error>

namespace omarchy {

namespace {

// Where the capture script writes the path of the recording in progress.
constexpr std::string_view kCurrentRecording = "/tmp/omarchy-screenrecord-filename";
// A capture that changed this recently is treated as still being written.
constexpr auto kSettle = std::chrono::seconds(5);

std::filesystem::path env_dir(const char* name) {
    const char* v = std::getenv(name); // NOLINT(concurrency-mt-unsafe): read on the UI thread
    return v != nullptr && *v != '\0' ? std::filesystem::path(v) : std::filesystem::path{};
}

bool is_video(const std::filesystem::path& p) {
    static constexpr std::array kExtensions{".mp4", ".mkv", ".webm", ".mov"};
    const std::string ext = p.extension().string();
    return std::ranges::any_of(kExtensions, [&](std::string_view e) { return ext == e; });
}

std::filesystem::path current_recording() {
    std::ifstream in{std::string(kCurrentRecording)};
    std::string line;
    std::getline(in, line);
    return line;
}

} // namespace

std::filesystem::path recordings_dir() {
    if (auto d = env_dir("OMARCHY_SCREENRECORD_DIR"); !d.empty()) return d;
    if (auto d = env_dir("XDG_VIDEOS_DIR"); !d.empty()) return d;
    return env_dir("HOME") / "Videos";
}

std::vector<Recording> list_recordings(const std::filesystem::path& dir, std::size_t limit) {
    std::vector<Recording> out;
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec) return out;
    const auto now = std::filesystem::file_time_type::clock::now();
    const std::filesystem::path current = current_recording();
    for (const auto& entry : it) {
        if (!entry.is_regular_file(ec) || !is_video(entry.path())) continue;
        Recording r{.path = entry.path(),
                    .modified = entry.last_write_time(ec),
                    .size = entry.file_size(ec),
                    .growing = false};
        if (ec) continue;
        r.growing = (!current.empty() && std::filesystem::equivalent(current, r.path, ec)) &&
                    now - r.modified < kSettle;
        out.push_back(std::move(r));
    }
    std::ranges::sort(out, std::ranges::greater{}, &Recording::modified);
    if (out.size() > limit) out.resize(limit);
    return out;
}

} // namespace omarchy
