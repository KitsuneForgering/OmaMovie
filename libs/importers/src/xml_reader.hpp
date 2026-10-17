#pragma once

// The cursor behind xml::parse (xml_parse.cpp, xml_lexer.cpp).

#include "xml.hpp"

#include <format>

namespace oma::importers::xml {

struct Reader {
    std::string_view text;
    std::size_t at;
    Limits limits;
    std::size_t elements;
    std::size_t failed_at; // furthest position reached, for the error

    [[nodiscard]] bool peek(std::string_view s) const noexcept {
        return text.substr(at).starts_with(s);
    }
    bool take(std::string_view s) noexcept {
        if (!peek(s))
            return false;
        at += s.size();
        return true;
    }
    void skip_space() noexcept;
    // Whitespace, comments and processing instructions; false on `<!DOCTYPE`, `<!ENTITY` or an
    // unterminated one.
    [[nodiscard]] bool skip_misc();
    [[nodiscard]] bool name(std::string& out);
    [[nodiscard]] bool attributes(Element& e);
    [[nodiscard]] bool element(Element& e, std::size_t depth);
    [[nodiscard]] std::unexpected<Error> fail() const {
        return make_error(ErrorCode::InvalidData, Category::Importer,
                          "malformed or unsupported XML", std::format("byte {}", at));
    }
};

} // namespace oma::importers::xml
