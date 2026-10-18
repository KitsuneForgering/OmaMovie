#include "diff_oracle.hpp"

#include <algorithm>
#include <random>
#include <string>

#include "oma_test.hpp"

namespace {
// 5,000 random pairs over a three-line alphabet: every script is minimal and correct.
bool matches_the_minimal_edit_count() {
    static const std::string kAlphabet[] = {"x", "y", "z"};
    std::mt19937 random(12345);
    for (int round = 0; round < 5000; ++round) {
        const auto draw = [&] {
            Lines out(random() % 12);
            std::ranges::generate(out, [&] { return std::string_view(kAlphabet[random() % 3]); });
            return out;
        };
        const Lines a = draw();
        const Lines b = draw();
        const auto edits = line_diff(a, b);
        if (!edits || edits->size() != a.size() + b.size() - (2 * lcs(a, b)) ||
            !rebuilds(a, b, *edits))
            return false;
    }
    return true;
}
} // namespace

void run_line_diff_tests() {
    describe("oma-project line diff", {
        it("finds a minimal, correct edit script",
           { expect(matches_the_minimal_edit_count()).toBeTruthy(); });
    });
}
