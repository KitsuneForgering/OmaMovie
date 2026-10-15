#include "xml_reader.hpp"

namespace oma::importers::xml {

namespace {
bool space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}
// Name characters: ASCII letters, digits, `_ : - .` and any byte of a UTF-8 sequence.
bool name_char(char c, bool first) {
    const auto u = static_cast<unsigned char>(c);
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == ':' || u >= 0x80 ||
           (!first && ((c >= '0' && c <= '9') || c == '-' || c == '.'));
}
} // namespace

void Reader::skip_space() noexcept {
    while (at < text.size() && space(text[at]))
        ++at;
}

bool Reader::name(std::string& out) {
    const std::size_t start = at;
    while (at < text.size() && name_char(text[at], at == start))
        ++at;
    out.assign(text.substr(start, at - start));
    return at > start;
}

} // namespace oma::importers::xml
