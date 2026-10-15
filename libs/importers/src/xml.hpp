#pragma once

// A small XML reader for the interchange formats OmaMovie imports (MLT/Kdenlive), safe by
// construction (CLAUDE.md §18): no DTD at all (`<!DOCTYPE` and `<!ENTITY` are refused, so there
// is no external entity and no entity expansion), only the five predefined entities and numeric
// character references, bounded depth, element count and size. Not a general XML parser:
// namespaces are kept as part of names, processing instructions and comments are skipped.

#include "oma/base/error.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace oma::importers::xml {

struct Element {
    std::string name;
    std::vector<std::pair<std::string, std::string>> attributes; // decoded values
    std::vector<Element> children;
    std::string text; // the element's own character data and CDATA, decoded, concatenated

    // The value of attribute `key`, or "" when absent.
    [[nodiscard]] std::string_view attribute(std::string_view key) const noexcept;
};

struct Limits {
    std::size_t max_bytes = std::size_t{64} << 20;
    std::size_t max_depth = 64;
    std::size_t max_elements = 1'000'000;
};

// The document's root element; an error names the byte offset of the first problem.
[[nodiscard]] Result<Element> parse(std::string_view text, const Limits& limits = {});

// `text` with `&lt; &gt; &amp; &quot; &apos; &#N; &#xN;` replaced (UTF-8); none for any other
// reference or a code point outside Unicode.
[[nodiscard]] bool decode(std::string_view text, std::string& out);

} // namespace oma::importers::xml
