#include "commands.hpp"

#include <algorithm>

// The edits along the path Myers found, walking the saved rows back from the end of both files.
std::vector<DiffLine> backtrack(const std::vector<std::vector<std::int32_t>>& trace,
                                const std::vector<std::string_view>& a,
                                const std::vector<std::string_view>& b) {
    std::vector<DiffLine> out;
    auto x = static_cast<std::int32_t>(a.size());
    auto y = static_cast<std::int32_t>(b.size());
    for (auto d = static_cast<std::int32_t>(trace.size()) - 1; d > 0; --d) {
        const auto& prev = trace[static_cast<std::size_t>(d - 1)];
        const auto at = [&](std::int32_t k) {
            return prev[static_cast<std::size_t>(k + d - 1)];
        };
        const std::int32_t k = x - y;
        const bool down = k == -d || (k != d && at(k - 1) < at(k + 1));
        const std::int32_t pk = down ? k + 1 : k - 1;
        const std::int32_t px = at(pk);
        const std::int32_t py = px - pk;
        if (down)
            out.push_back({'+', b[static_cast<std::size_t>(py)], static_cast<std::size_t>(py) + 1});
        else
            out.push_back({'-', a[static_cast<std::size_t>(px)], static_cast<std::size_t>(px) + 1});
        x = px;
        y = py;
    }
    std::reverse(out.begin(), out.end());
    return out;
}
