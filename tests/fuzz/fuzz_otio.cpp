// libFuzzer target for OpenTimelineIO JSON (CLAUDE.md §18, ADR-0008): any input gives a handled
// error or ProjectIR that passes validation, never a crash, hang or unbounded allocation.

#include "oma/importers/otio.hpp"
#include "oma/project_ir/validate.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    auto imported = oma::importers::import_otio(text);
    if (imported && !oma::ir::validate(imported->timeline)) {
        __builtin_trap();
    }
    return 0;
}
