// libFuzzer target for the project loader (CLAUDE.md §18): any input must give a document whose
// timeline passes validation and saves again, or a handled error; never a crash, UB or unbounded
// work. Run with `make fuzz`.

#include "oma/project/document.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    auto doc = oma::project::from_json(text, "/fuzz");
    if (doc) {
        if (doc->timeline && !doc->timeline->validate()) {
            __builtin_trap(); // restore() let an invalid timeline through
        }
        if (!oma::project::to_json(*doc, "/fuzz")) {
            __builtin_trap();
        }
    }
    return 0;
}
