#include "fcpxml_detail.hpp"

#include <charconv>
#include <format>

namespace oma::importers::fcpxml {

std::optional<Rational> seconds(std::string_view text) {
    if (!text.ends_with('s'))
        return std::nullopt;
    text.remove_suffix(1);
    const auto slash = text.find('/');
    std::int64_t num = 0;
    std::int64_t den = 1;
    const auto read = [](std::string_view s, std::int64_t& v) {
        return !s.empty() && std::from_chars(s.begin(), s.end(), v).ptr == s.end();
    };
    if (!read(text.substr(0, slash), num) ||
        (slash != std::string_view::npos && !read(text.substr(slash + 1), den)))
        return std::nullopt;
    auto r = Rational::make(num, den);
    return r && r->den() <= 1'000'000'000 ? std::optional(*r) : std::nullopt;
}

RationalTime on_grid(Context& c, std::string_view text, std::string_view where) {
    const auto s = seconds(text);
    if (!s) {
        if (!text.empty())
            issue(c.out.report, std::string(where), std::format("time \"{}\" not read", text),
                  "read as zero");
        return *RationalTime::make(0, c.frame);
    }
    // s seconds in frames of c.frame seconds: s / frame, to the nearest frame.
    const auto exact = RationalTime::make(s->num(), Rational::make(1, s->den()).value_or(c.frame));
    const auto frames = exact ? exact->rescaled(c.frame, Rounding::Nearest) : exact;
    if (!frames) {
        issue(c.out.report, std::string(where), std::format("time {} out of range", text),
              "read as zero");
        return *RationalTime::make(0, c.frame);
    }
    if (!(*frames == *exact))
        issue(c.out.report, std::string(where), std::format("time {} is off the frame grid", text),
              "rounded to the nearest frame");
    return *frames;
}

RationalTime add(const RationalTime& a, const RationalTime& b) {
    return a.plus(b).value_or(a);
}

RationalTime sub(const RationalTime& a, const RationalTime& b) {
    return a.minus(b).value_or(a);
}

} // namespace oma::importers::fcpxml
