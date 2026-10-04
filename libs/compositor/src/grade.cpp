#include "oma/compositor/grade.hpp"

#include <charconv>
#include <cmath>
#include <format>
#include <string>

namespace oma::compositor {

namespace {

Error invalid(std::string message, std::string context = {}) {
    return {ErrorCode::InvalidData, Category::Compositor, std::move(message), std::move(context)};
}

bool in_range(double v, double lo, double hi) {
    return std::isfinite(v) && v >= lo && v <= hi;
}

Result<void> check_curve(std::span<const CurvePoint> points, std::string_view name) {
    if (points.empty()) {
        return {};
    }
    if (points.size() < 2 || points.size() > kMaxCurvePoints) {
        return std::unexpected(invalid("a curve needs 2 to 16 points", std::string(name)));
    }
    for (std::size_t i = 0; i < points.size(); ++i) {
        const CurvePoint& p = points[i];
        if (!in_range(p.x, 0.0, 1.0) || !in_range(p.y, 0.0, 1.0) ||
            (i > 0 && p.x <= points[i - 1].x)) {
            return std::unexpected(
                invalid("curve points must lie in [0, 1] with increasing x", std::string(name)));
        }
    }
    return {};
}

// --- .cube parsing

std::string_view trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
        return {};
    }
    return s.substr(first, s.find_last_not_of(" \t\r") - first + 1);
}

// Reads `count` numbers separated by blanks, nothing else on the line.
template <std::size_t N>
bool numbers(std::string_view s, std::array<double, N>& out) {
    for (std::size_t i = 0; i < N; ++i) {
        s = trim(s);
        const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), out[i]);
        if (ec != std::errc{} || !std::isfinite(out[i])) {
            return false;
        }
        s.remove_prefix(static_cast<std::size_t>(end - s.data()));
        if (!s.empty() && s.front() != ' ' && s.front() != '\t' && s.front() != '\r') {
            return false;
        }
    }
    return trim(s).empty();
}

} // namespace

bool Cdl::identity() const noexcept {
    return *this == Cdl{};
}

bool Curves::identity() const noexcept {
    return master.empty() && red.empty() && green.empty() && blue.empty();
}

bool Grade::identity() const noexcept {
    return cdl.identity() && curves.identity() && (lut == nullptr || lut_amount == 0.0);
}

std::vector<double> curve_tangents(std::span<const CurvePoint> points) {
    const std::size_t n = points.size();
    std::vector<double> m(n, 0.0);
    if (n < 2) {
        return m;
    }
    std::vector<double> d(n - 1);
    for (std::size_t k = 0; k + 1 < n; ++k) {
        d[k] = (points[k + 1].y - points[k].y) / (points[k + 1].x - points[k].x);
    }
    m[0] = d[0];
    m[n - 1] = d[n - 2];
    for (std::size_t k = 1; k + 1 < n; ++k) {
        m[k] = d[k - 1] * d[k] > 0.0 ? (d[k - 1] + d[k]) / 2.0 : 0.0;
    }
    // Fritsch–Carlson: limit the tangents so each segment stays monotone.
    for (std::size_t k = 0; k + 1 < n; ++k) {
        if (d[k] == 0.0) {
            m[k] = 0.0;
            m[k + 1] = 0.0;
            continue;
        }
        const double a = m[k] / d[k];
        const double b = m[k + 1] / d[k];
        const double s = (a * a) + (b * b);
        if (s > 9.0) {
            const double t = 3.0 / std::sqrt(s);
            m[k] = t * a * d[k];
            m[k + 1] = t * b * d[k];
        }
    }
    return m;
}

double evaluate_curve(std::span<const CurvePoint> points, double x) {
    if (points.empty()) {
        return x;
    }
    if (x <= points.front().x) {
        return points.front().y;
    }
    if (x >= points.back().x) {
        return points.back().y;
    }
    const auto m = curve_tangents(points);
    std::size_t k = 0;
    while (k + 2 < points.size() && x >= points[k + 1].x) {
        ++k;
    }
    const CurvePoint& p0 = points[k];
    const CurvePoint& p1 = points[k + 1];
    const double h = p1.x - p0.x;
    const double t = (x - p0.x) / h;
    const double t2 = t * t;
    const double t3 = t2 * t;
    return (((2.0 * t3) - (3.0 * t2) + 1.0) * p0.y) + ((t3 - (2.0 * t2) + t) * h * m[k]) +
           (((-2.0 * t3) + (3.0 * t2)) * p1.y) + ((t3 - t2) * h * m[k + 1]);
}

Result<void> validate(const Grade& grade) {
    const Cdl& c = grade.cdl;
    for (std::size_t i = 0; i < 3; ++i) {
        if (!in_range(c.slope[i], 0.0, 4.0) || !in_range(c.offset[i], -1.0, 1.0) ||
            !in_range(c.power[i], 0.1, 4.0)) {
            return std::unexpected(invalid("CDL slope, offset or power out of range"));
        }
    }
    if (!in_range(c.saturation, 0.0, 4.0)) {
        return std::unexpected(invalid("CDL saturation out of range"));
    }
    for (const auto& [points, name] :
         {std::pair{&grade.curves.master, "master curve"},
          std::pair{&grade.curves.red, "red curve"}, std::pair{&grade.curves.green, "green curve"},
          std::pair{&grade.curves.blue, "blue curve"}}) {
        if (auto r = check_curve(*points, name); !r) {
            return r;
        }
    }
    if (!in_range(grade.lut_amount, 0.0, 1.0)) {
        return std::unexpected(invalid("LUT amount out of range"));
    }
    if (const Lut3d* lut = grade.lut.get()) {
        const std::size_t n = lut->size;
        if (n < 2 || n > kMaxLutSize || lut->rgb.size() != n * n * n * 3) {
            return std::unexpected(invalid("malformed 3D LUT"));
        }
        for (std::size_t i = 0; i < 3; ++i) {
            if (!(lut->domain_max[i] > lut->domain_min[i])) {
                return std::unexpected(invalid("empty 3D LUT domain"));
            }
        }
    }
    return {};
}

Result<Lut3d> parse_cube(std::string_view text) {
    if (text.size() > kMaxCubeBytes) {
        return std::unexpected(invalid("the .cube file is too large"));
    }
    Lut3d lut;
    std::size_t expected = 0; // rows
    std::size_t line_no = 0;
    while (!text.empty()) {
        const auto eol = text.find('\n');
        const std::string_view raw = text.substr(0, eol);
        text.remove_prefix(eol == std::string_view::npos ? text.size() : eol + 1);
        ++line_no;
        const std::string_view line = trim(raw);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const auto where = [&] {
            return std::format("line {}", line_no);
        };
        const char first = line.front();
        if ((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z')) {
            if (!lut.rgb.empty()) {
                return std::unexpected(invalid("keyword after the table data", where()));
            }
            const auto space = line.find_first_of(" \t");
            const std::string_view key = line.substr(0, space);
            const std::string_view rest =
                space == std::string_view::npos ? std::string_view{} : line.substr(space);
            if (key == "LUT_3D_SIZE") {
                std::array<double, 1> v{};
                if (!numbers(rest, v) || v[0] < 2.0 || v[0] > kMaxLutSize ||
                    v[0] != std::floor(v[0])) {
                    return std::unexpected(
                        invalid(std::format("LUT_3D_SIZE must be 2 to {}", kMaxLutSize), where()));
                }
                lut.size = static_cast<std::uint32_t>(v[0]);
                expected = static_cast<std::size_t>(lut.size) * lut.size * lut.size;
                lut.rgb.reserve(expected * 3);
            } else if (key == "LUT_1D_SIZE") {
                return std::unexpected(Error(ErrorCode::Unsupported, Category::Compositor,
                                             "1D LUTs are not supported", where()));
            } else if (key == "DOMAIN_MIN" || key == "DOMAIN_MAX") {
                std::array<double, 3> v{};
                if (!numbers(rest, v)) {
                    return std::unexpected(invalid("DOMAIN needs three numbers", where()));
                }
                auto& target = key == "DOMAIN_MIN" ? lut.domain_min : lut.domain_max;
                for (std::size_t i = 0; i < 3; ++i) {
                    target[i] = static_cast<float>(v[i]);
                }
            } else if (key == "LUT_3D_INPUT_RANGE") {
                std::array<double, 2> v{};
                if (!numbers(rest, v)) {
                    return std::unexpected(
                        invalid("LUT_3D_INPUT_RANGE needs two numbers", where()));
                }
                lut.domain_min.fill(static_cast<float>(v[0]));
                lut.domain_max.fill(static_cast<float>(v[1]));
            }
            // TITLE and other keywords carry nothing the table needs.
            continue;
        }
        if (expected == 0) {
            return std::unexpected(invalid("table data before LUT_3D_SIZE", where()));
        }
        std::array<double, 3> v{};
        if (!numbers(line, v)) {
            return std::unexpected(invalid("a table row needs three numbers", where()));
        }
        if (lut.rgb.size() == expected * 3) {
            return std::unexpected(invalid("more table rows than LUT_3D_SIZE^3", where()));
        }
        for (const double c : v) {
            lut.rgb.push_back(static_cast<float>(c));
        }
    }
    if (expected == 0) {
        return std::unexpected(invalid("no LUT_3D_SIZE"));
    }
    if (lut.rgb.size() != expected * 3) {
        return std::unexpected(invalid(
            std::format("{} table rows, LUT_3D_SIZE needs {}", lut.rgb.size() / 3, expected)));
    }
    for (std::size_t i = 0; i < 3; ++i) {
        if (!(lut.domain_max[i] > lut.domain_min[i])) {
            return std::unexpected(invalid("DOMAIN_MAX must exceed DOMAIN_MIN"));
        }
    }
    return lut;
}

} // namespace oma::compositor
