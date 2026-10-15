#include "xml_reader.hpp"

namespace oma::importers::xml {

bool Reader::skip_misc() {
    for (;;) {
        skip_space();
        if (peek("<!DOCTYPE") || peek("<!ENTITY"))
            return false; // no DTD: no external entities, no entity expansion
        const bool comment = peek("<!--");
        if (!comment && !peek("<?"))
            return true;
        const std::size_t end = text.find(comment ? "-->" : "?>", at + 2);
        if (end == std::string_view::npos)
            return false;
        at = end + (comment ? 3 : 2);
    }
}

bool Reader::attributes(Element& e) {
    for (;;) {
        const std::size_t before = at;
        skip_space();
        std::string key;
        if (at == before || !name(key)) {
            at = before;
            return true; // `>` or `/>` comes next
        }
        skip_space();
        if (!take("="))
            return false;
        skip_space(); // `key =` then the end of the text: no quote to read
        const char quote = at < text.size() ? text[at] : '\0';
        const std::size_t end =
            quote == '"' || quote == '\'' ? text.find(quote, at + 1) : text.npos;
        std::string value;
        const std::string_view raw =
            end == std::string_view::npos ? std::string_view{} : text.substr(at + 1, end - at - 1);
        if (end == std::string_view::npos || raw.find('<') != std::string_view::npos ||
            !decode(raw, value))
            return false;
        e.attributes.emplace_back(std::move(key), std::move(value));
        at = end + 1;
    }
}

} // namespace oma::importers::xml
