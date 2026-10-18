// Myers' shortest edit script over lines ("An O(ND) Difference Algorithm and Its Variations",
// 1986): O((N + M) D) time, cheap when two projects differ in a few fields, the reverse-
// engineering case. Each step keeps only its diagonals [-d, d], so memory is O(D²); past
// kMaxEdits the files are reported as too different instead.
#include "commands.hpp"

#include <algorithm>

std::optional<std::vector<DiffLine>> line_diff(const std::vector<std::string_view>& a,
                                               const std::vector<std::string_view>& b) {
    using Row = std::vector<std::int32_t>; // furthest x on diagonals -d..d, index k + d
    const auto n = static_cast<std::int32_t>(a.size());
    const auto m = static_cast<std::int32_t>(b.size());
    std::vector<Row> trace;
    Row prev{0}; // a virtual d = -1 row: diagonal 1 starts at x = 0
    for (std::int32_t d = 0; d <= std::min<std::int32_t>(n + m, kMaxEdits); ++d) {
        Row row(static_cast<std::size_t>(2 * d + 1));
        const auto from = [&](std::int32_t k) {
            return prev[static_cast<std::size_t>(k + d - 1)];
        };
        for (std::int32_t k = -d; k <= d; k += 2) {
            const bool down = d == 0 || k == -d || (k != d && from(k - 1) < from(k + 1));
            std::int32_t x = d == 0 ? 0 : (down ? from(k + 1) : from(k - 1) + 1);
            std::int32_t y = x - k;
            while (x < n && y < m &&
                   a[static_cast<std::size_t>(x)] == b[static_cast<std::size_t>(y)])
                ++x, ++y;
            row[static_cast<std::size_t>(k + d)] = x;
            if (x >= n && y >= m) {
                trace.push_back(std::move(row));
                return backtrack(trace, a, b);
            }
        }
        trace.push_back(row);
        prev = std::move(row);
    }
    return std::nullopt;
}
