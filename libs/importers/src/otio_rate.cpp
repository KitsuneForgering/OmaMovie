#include "otio_detail.hpp"

#include <array>
#include <cmath>
#include <format>

namespace oma::importers::otio {

// OTIO rates are doubles (23.976023976023978 is 24000/1001): whole and NTSC rates are exact,
// others keep three decimals and are reported.
Result<Rational> rate_of(double r, Context& c, std::string_view where) {
    if (!std::isfinite(r) || r <= 0.0 || r > 1e6)
        return make_error(ErrorCode::InvalidData, Category::Importer, "invalid rate",
                          std::string(where));
    static constexpr std::array kNtsc{24000, 30000, 48000, 60000, 120000};
    for (const int num : kNtsc)
        if (std::abs(r - (num / 1001.0)) < 1e-6)
            return Rational::make(num, 1001);
    if (std::abs(r - std::round(r)) < 1e-9)
        return Rational::make(static_cast<std::int64_t>(std::round(r)), 1);
    c.issue(std::string(where), std::format("rate {} has no exact fraction", r),
            "rounded to 1/1000");
    return Rational::make(static_cast<std::int64_t>(std::round(r * 1000.0)), 1000);
}

} // namespace oma::importers::otio
