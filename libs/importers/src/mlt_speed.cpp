#include "mlt_detail.hpp"

#include <charconv>

namespace oma::importers::mlt {

std::optional<Rational> speed_of(std::string_view value) {
    // A decimal as MLT prints a double: `2`, `0.5`, `-1`, `1.25`; read exactly, never rounded.
    const bool negative = value.starts_with('-');
    const std::string_view digits = value.substr(negative ? 1 : 0);
    const std::size_t point = digits.find('.');
    const std::string_view whole = digits.substr(0, point);
    const std::string_view fraction =
        point == std::string_view::npos ? std::string_view() : digits.substr(point + 1);
    if (whole.empty() || whole.size() + fraction.size() > 6) // Kdenlive: ±100, two decimals
        return std::nullopt;
    std::int64_t num = 0;
    std::int64_t den = 1;
    for (const std::string_view part : {whole, fraction})
        for (const char c : part) {
            if (c < '0' || c > '9')
                return std::nullopt;
            num = (num * 10) + (c - '0');
        }
    for (std::size_t i = 0; i < fraction.size(); ++i)
        den *= 10;
    const auto speed = Rational::make(negative ? -num : num, den);
    return speed ? std::optional(*speed) : std::nullopt;
}

} // namespace oma::importers::mlt
