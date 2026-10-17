#include "xml_reader.hpp"

#include <format>

namespace oma::importers::xml {

std::string_view Element::attribute(std::string_view key) const noexcept {
    for (const auto& [k, v] : attributes)
        if (k == key)
            return v;
    return {};
}

Result<Element> parse(std::string_view text, const Limits& limits) {
    if (text.size() > limits.max_bytes)
        return make_error(ErrorCode::InvalidData, Category::Importer, "XML larger than allowed");
    Reader r{.text = text, .at = 0, .limits = limits, .elements = 0, .failed_at = 0};
    if (!r.skip_misc() || !r.peek("<"))
        return r.fail();
    Element root;
    if (!r.element(root, 1) || !r.skip_misc() || r.at != text.size())
        return r.fail();
    return root;
}

bool Reader::element(Element& e, std::size_t depth) {
    if (depth > limits.max_depth || ++elements > limits.max_elements || !take("<") ||
        !name(e.name) || !attributes(e))
        return false;
    skip_space();
    if (take("/>"))
        return true;
    if (!take(">"))
        return false;
    for (;;) {
        const std::size_t lt = text.find('<', at);
        if (lt == std::string_view::npos || !decode(text.substr(at, lt - at), e.text))
            return false;
        at = lt;
        if (take("</")) {
            std::string closing;
            skip_space();
            return name(closing) && closing == e.name && (skip_space(), take(">"));
        }
        if (peek("<!--") || peek("<?")) {
            if (!skip_misc())
                return false;
        } else if (take("<![CDATA[")) {
            const std::size_t end = text.find("]]>", at);
            if (end == std::string_view::npos)
                return false;
            e.text.append(text.substr(at, end - at));
            at = end + 3;
        } else if (!element(e.children.emplace_back(), depth + 1)) {
            return false;
        }
    }
}

} // namespace oma::importers::xml
