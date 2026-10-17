#pragma once

// External data an importer could not represent, kept for a later round trip (CLAUDE.md §16.3):
// never dropped silently.

#include <string>
#include <utility>
#include <vector>

namespace oma::ir {

struct Opaque {
    std::string format;      // e.g. "otio"
    std::string original_id; // the object's name or id in the source, if any
    std::string schema;      // e.g. "LinearTimeWarp.1"
    std::string raw;         // the source text of the object, when feasible
    std::vector<std::pair<std::string, std::string>> fields; // what was understood of it
};

} // namespace oma::ir
