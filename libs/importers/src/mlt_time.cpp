#include "mlt_detail.hpp"

#include <charconv>
#include <cmath>

namespace oma::importers::mlt {

namespace {
bool number(std::string_view s, std::int64_t& v) {
    return !s.empty() && std::from_chars(s.begin(), s.end(), v).ptr == s.end() && v >= 0;
}
} // namespace

std::optional<std::int64_t> frames_of(std::string_view value, const Document& doc) {
    std::int64_t frames = 0;
    if (number(value, frames))
        return frames;
    // HH:MM:SS.mmm (clock) or HH:MM:SS:FF / HH:MM:SS;FF (timecode).
    if (value.size() < 8 || value[2] != ':' || value[5] != ':')
        return std::nullopt;
    std::int64_t h = 0;
    std::int64_t m = 0;
    std::int64_t s = 0;
    if (!number(value.substr(0, 2), h) || !number(value.substr(3, 2), m) ||
        !number(value.substr(6, 2), s) || m > 59 || s > 59)
        return std::nullopt;
    const std::int64_t whole = (((h * 60) + m) * 60) + s;
    const std::string_view rest = value.substr(8);
    if (rest.empty())
        return whole * doc.rate.num() / doc.rate.den();
    if (rest[0] == '.') {
        double fraction = 0;
        const std::string_view digits = rest.substr(1);
        const auto [end, err] = std::from_chars(digits.begin(), digits.end(), fraction);
        if (err != std::errc() || end != digits.end() || digits.size() > 9)
            return std::nullopt;
        fraction /= std::pow(10.0, static_cast<double>(digits.size()));
        // Seconds at the profile's rate, to the nearest frame (MLT writes them that way).
        const double at = (static_cast<double>(whole) + fraction) *
                          static_cast<double>(doc.rate.num()) / static_cast<double>(doc.rate.den());
        return static_cast<std::int64_t>(std::llround(at));
    }
    if ((rest[0] == ':' || rest[0] == ';') && number(rest.substr(1), frames) && frames < doc.fps)
        return (whole * doc.fps) + frames;
    return std::nullopt;
}

} // namespace oma::importers::mlt
