#pragma once

// An independent check for the Myers diff: the textbook dynamic-programming LCS, and whether
// an edit script is consistent with both inputs.

#include "commands.hpp"

#include <algorithm>

using Lines = std::vector<std::string_view>;

inline std::size_t lcs(const Lines& a, const Lines& b) {
    std::vector<std::vector<std::size_t>> t(a.size() + 1,
                                            std::vector<std::size_t>(b.size() + 1, 0));
    for (std::size_t i = 1; i <= a.size(); ++i)
        for (std::size_t j = 1; j <= b.size(); ++j)
            t[i][j] =
                a[i - 1] == b[j - 1] ? t[i - 1][j - 1] + 1 : std::max(t[i - 1][j], t[i][j - 1]);
    return t[a.size()][b.size()];
}

// The lines neither deleted from `a` nor inserted into `b` must be the same sequence.
inline bool rebuilds(const Lines& a, const Lines& b, const std::vector<DiffLine>& edits) {
    std::vector<bool> deleted(a.size(), false);
    std::vector<bool> inserted(b.size(), false);
    for (const DiffLine& d : edits)
        (d.op == '-' ? deleted : inserted)[d.line - 1] = true;
    const auto kept = [](const Lines& lines, const std::vector<bool>& edited) {
        Lines out;
        for (std::size_t i = 0; i < lines.size(); ++i)
            if (!edited[i])
                out.push_back(lines[i]);
        return out;
    };
    return kept(a, deleted) == kept(b, inserted);
}
