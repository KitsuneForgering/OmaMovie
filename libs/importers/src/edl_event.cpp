#include "edl_detail.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>

namespace oma::importers::edl {

Result<Event> parse_event(std::vector<std::string_view> w, std::int64_t fps, bool& drop,
                          std::size_t line) {
    if (w.size() > 4 && w[3].starts_with('K') && (w[4] == "B" || w[4] == "O"))
        w.erase(w.begin() + 4); // a key's background/overlay mark: kept in the kind below
    const auto bad = [&] {
        return make_error(ErrorCode::InvalidData, Category::Importer, "malformed EDL event",
                          std::format("line {}", line));
    };
    const bool timed = w.size() == 9; // a transition length before the timecodes
    if ((w.size() != 8 && !timed) ||
        !std::ranges::all_of(w[0], [](char c) { return c >= '0' && c <= '9'; }))
        return bad();
    Event e;
    e.number = std::string(w[0]);
    e.reel = std::string(w[1]);
    e.channels = std::string(w[2]);
    e.kind = std::string(w[3]);
    e.line = line;
    if (timed &&
        std::from_chars(w[4].data(), w[4].data() + w[4].size(), e.transition).ec != std::errc())
        return bad();
    const std::size_t at = timed ? 5 : 4;
    drop = drop || (w[at].size() == 11 && w[at][8] == ';');
    std::array<std::int64_t*, 4> into{&e.src_in, &e.src_out, &e.rec_in, &e.rec_out};
    for (std::size_t i = 0; i < 4; ++i) {
        const auto f = frames_of(w[at + i], fps, drop);
        if (!f)
            return bad();
        *into[i] = *f;
    }
    if (e.src_out < e.src_in || e.rec_out < e.rec_in)
        return bad();
    return e;
}

} // namespace oma::importers::edl
