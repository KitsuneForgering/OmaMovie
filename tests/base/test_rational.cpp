#include "oma/base/rational.hpp"

#include <cstdint>
#include <limits>

#include "oma_test.hpp"

using oma::ErrorCode;
using oma::Rational;
using oma::Rounding;

namespace {

constexpr Rational kSecond = Rational::literal(1, 1);
constexpr Rational kTb90k = Rational::literal(1, 90000);
constexpr Rational kTb48k = Rational::literal(1, 48000);
constexpr Rational kFrame2997 = Rational::literal(1001, 30000);
constexpr Rational kFrame23976 = Rational::literal(1001, 24000);

// Literals are normalized at compile time.
static_assert(Rational::literal(2, 4) == Rational::literal(1, 2));
static_assert(Rational::literal(3, -6).num() == -1);
static_assert(Rational::literal(3, -6).den() == 2);

constexpr auto kMax = std::numeric_limits<std::int64_t>::max();
constexpr auto kMin = std::numeric_limits<std::int64_t>::min();

std::int64_t rescale_ok(std::int64_t v, Rational from, Rational to, Rounding r) {
    return *oma::rescale(v, from, to, r);
}

ErrorCode rescale_error(std::int64_t v, Rational from, Rational to) {
    return oma::rescale(v, from, to, Rounding::Nearest).error().code();
}

} // namespace

void run_rational_tests() {
    describe("Rational", {
        it("normalizes sign and common factors", {
            auto r = Rational::make(6, -8);
            expect(r.has_value()).toBeTruthy();
            expect(r->num()).toEqual(-3);
            expect(r->den()).toEqual(4);
        });

        it("rejects a zero denominator", {
            auto r = Rational::make(1, 0);
            expect(r.has_value()).toBeFalsy();
            expect(static_cast<int>(r.error().code()))
                .toEqual(static_cast<int>(ErrorCode::InvalidArgument));
        });

        it("rejects INT64_MIN because it cannot be negated", {
            expect(Rational::make(kMin, 1).has_value()).toBeFalsy();
            expect(Rational::make(1, kMin).has_value()).toBeFalsy();
        });

        it("orders fractions exactly", {
            expect(Rational::literal(1, 3) < Rational::literal(1, 2)).toBeTruthy();
            expect(Rational::literal(-1, 2) < Rational::literal(1, 3)).toBeTruthy();
            expect(Rational::literal(30000, 1001) > Rational::literal(29, 1)).toBeTruthy();
            expect(Rational::literal(kMax, 1) > Rational::literal(kMax - 1, 1)).toBeTruthy();
            expect(Rational::literal(kMax, kMax - 1) > Rational::literal(kMax - 1, kMax - 2))
                .toBeFalsy();
        });

        it("inverts and refuses to invert zero", {
            expect(Rational::literal(1001, 30000).inverse()->num()).toEqual(30000);
            expect(Rational::literal(0, 1).inverse().has_value()).toBeFalsy();
        });
    });

    describe("rescale", {
        it("converts seconds to a 90 kHz timebase exactly",
           { expect(rescale_ok(1, kSecond, kTb90k, Rounding::Floor)).toEqual(90000); });

        it("converts one 29.97 fps frame to 90 kHz exactly (3003 ticks)",
           { expect(rescale_ok(1, kFrame2997, kTb90k, Rounding::Floor)).toEqual(3003); });

        it("rounds one 23.976 fps frame (3753.75 ticks) as requested", {
            expect(rescale_ok(1, kFrame23976, kTb90k, Rounding::Floor)).toEqual(3753);
            expect(rescale_ok(1, kFrame23976, kTb90k, Rounding::Ceil)).toEqual(3754);
            expect(rescale_ok(1, kFrame23976, kTb90k, Rounding::Nearest)).toEqual(3754);
        });

        it("rounds negative values toward the right direction", {
            expect(rescale_ok(-1, kFrame23976, kTb90k, Rounding::Floor)).toEqual(-3754);
            expect(rescale_ok(-1, kFrame23976, kTb90k, Rounding::Ceil)).toEqual(-3753);
            expect(rescale_ok(-1, kFrame23976, kTb90k, Rounding::Nearest)).toEqual(-3754);
        });

        it("breaks ties away from zero with Nearest", {
            expect(rescale_ok(1, Rational::literal(1, 2), kSecond, Rounding::Nearest)).toEqual(1);
            expect(rescale_ok(-1, Rational::literal(1, 2), kSecond, Rounding::Nearest)).toEqual(-1);
            expect(rescale_ok(3, Rational::literal(1, 2), kSecond, Rounding::Nearest)).toEqual(2);
        });

        it("maps video frames to audio samples (29.97 fps at 48 kHz = 1601.6 samples)", {
            expect(rescale_ok(1, kFrame2997, kTb48k, Rounding::Nearest)).toEqual(1602);
            expect(rescale_ok(5, kFrame2997, kTb48k, Rounding::Nearest)).toEqual(8008);
            expect(rescale_ok(1, kFrame2997, Rational::literal(1, 44100), Rounding::Floor))
                .toEqual(1471);
        });

        it("handles ten hours of media in a 90 kHz timebase", {
            const std::int64_t ten_hours = 10LL * 3600 * 90000;
            expect(rescale_ok(ten_hours, kTb90k, kTb48k, Rounding::Floor))
                .toEqual(10LL * 3600 * 48000);
            expect(rescale_ok(ten_hours, kTb90k, kFrame2997, Rounding::Floor)).toEqual(1078921);
        });

        it("reports overflow instead of truncating", {
            expect(static_cast<int>(rescale_error(kMax, kSecond, kTb90k)))
                .toEqual(static_cast<int>(ErrorCode::Overflow));
            expect(rescale_ok(kMax, kTb90k, kSecond, Rounding::Floor)).toEqual(kMax / 90000);
        });

        it("rejects non-positive units", {
            expect(static_cast<int>(rescale_error(1, Rational::literal(0, 1), kSecond)))
                .toEqual(static_cast<int>(ErrorCode::InvalidArgument));
            expect(static_cast<int>(rescale_error(1, kSecond, Rational::literal(-1, 25))))
                .toEqual(static_cast<int>(ErrorCode::InvalidArgument));
        });
    });
}
