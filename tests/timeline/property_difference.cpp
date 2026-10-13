#include "property_state.hpp"

#include <format>
#include <sstream>

namespace property {

std::string difference(const State& expected, const State& actual) {
    if (expected == actual)
        return {};
    std::istringstream e(expected.text);
    std::istringstream a(actual.text);
    std::string le;
    std::string la;
    for (int line = 1;; ++line) {
        const bool more_e = static_cast<bool>(std::getline(e, le));
        const bool more_a = static_cast<bool>(std::getline(a, la));
        if (!more_e && !more_a)
            break;
        if (!more_e || !more_a || le != la)
            return std::format("line {}: expected '{}', got '{}'", line, more_e ? le : "<end>",
                               more_a ? la : "<end>");
    }
    for (std::size_t i = 0; i < expected.video.size() && i < actual.video.size(); ++i)
        if (expected.video[i] != actual.video[i] || expected.audio[i] != actual.audio[i])
            return std::format("properties of clip #{} differ", i);
    return "clip properties differ";
}

} // namespace property
