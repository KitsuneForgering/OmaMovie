#include "xml.hpp"

#include <array>
#include <charconv>
#include <cstdint>

namespace oma::importers::xml {

namespace {

void append_utf8(std::uint32_t cp, std::string& out) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
        return;
    }
    static constexpr std::array<std::uint32_t, 4> kLead{0, 0xC0, 0xE0, 0xF0};
    const int extra = cp < 0x800 ? 1 : (cp < 0x10000 ? 2 : 3);
    out += static_cast<char>(kLead[static_cast<std::size_t>(extra)] | (cp >> (6 * extra)));
    for (int i = extra - 1; i >= 0; --i)
        out += static_cast<char>(0x80 | ((cp >> (6 * i)) & 0x3F));
}

bool reference(std::string_view name, std::string& out) {
    static constexpr std::array<std::pair<std::string_view, char>, 5> kNamed{
        {{"lt", '<'}, {"gt", '>'}, {"amp", '&'}, {"quot", '"'}, {"apos", '\''}}};
    for (const auto& [n, c] : kNamed)
        if (name == n)
            return out += c, true;
    if (name.size() < 2 || name[0] != '#')
        return false;
    const bool hex = name[1] == 'x';
    const std::string_view digits = name.substr(hex ? 2 : 1);
    std::uint32_t cp = 0;
    const auto [end, err] = std::from_chars(digits.begin(), digits.end(), cp, hex ? 16 : 10);
    if (err != std::errc() || end != digits.end() || cp == 0 || cp > 0x10FFFF ||
        (cp >= 0xD800 && cp <= 0xDFFF))
        return false;
    append_utf8(cp, out);
    return true;
}

} // namespace

bool decode(std::string_view text, std::string& out) {
    for (std::size_t at = 0; at < text.size();) {
        const std::size_t amp = text.find('&', at);
        out.append(text.substr(at, amp - at));
        if (amp == std::string_view::npos)
            return true;
        const std::size_t semi = text.find(';', amp);
        if (semi == std::string_view::npos || semi - amp > 10 ||
            !reference(text.substr(amp + 1, semi - amp - 1), out))
            return false;
        at = semi + 1;
    }
    return true;
}

} // namespace oma::importers::xml
