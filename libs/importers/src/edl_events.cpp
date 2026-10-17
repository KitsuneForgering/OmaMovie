#include "edl_detail.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <ranges>

namespace oma::importers::edl {

Result<std::vector<Event>> parse_events(std::string_view text, std::int64_t fps, bool& drop,
                                        std::string& title, std::vector<std::string>& unread) {
    std::vector<Event> out;
    std::size_t n = 0;
    for (auto raw : text | std::views::split('\n')) {
        std::string_view line(raw.begin(), raw.end());
        ++n;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            line.remove_suffix(1);
        if (line.size() > 4096)
            return make_error(ErrorCode::InvalidData, Category::Importer, "EDL line too long",
                              std::format("line {}", n));
        if (line.empty())
            continue;
        if (take(line, "TITLE:")) {
            title = std::string(line);
        } else if (take(line, "FCM:")) {
            drop = line.starts_with("DROP");
        } else if (line.front() >= '0' && line.front() <= '9') {
            if (out.size() >= kMaxEdlEvents)
                return make_error(ErrorCode::OutOfRange, Category::Importer, "too many EDL events");
            auto e = parse_event(words(line), fps, drop, n);
            if (!e)
                return std::unexpected(e.error());
            out.push_back(std::move(*e));
        } else if (out.empty()) {
            unread.emplace_back(line);
        } else {
            std::string_view note = line;
            if (take(note, "*") && take(note, "FROM CLIP NAME:"))
                out.back().clip_name = std::string(note);
            else if (note = line; take(note, "*") && take(note, "SOURCE FILE:"))
                out.back().source_file = std::string(note);
            else
                out.back().notes.emplace_back(line);
        }
    }
    return out;
}

} // namespace oma::importers::edl
