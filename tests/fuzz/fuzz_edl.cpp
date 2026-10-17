// libFuzzer target for CMX 3600 EDL (CLAUDE.md §18, ADR-0008): any input gives a handled
// error or ProjectIR that passes validation, never a crash, hang or unbounded allocation.

#include "oma/base/log.hpp"
#include "oma/importers/edl.hpp"
#include "oma/project_ir/validate.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

// Quiet logging: a warning per input would cost more than the input itself and drown the
// fuzzer's own output; the code under test runs the same.
extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
    oma::set_log_level_all(oma::LogLevel::Off);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    auto imported = oma::importers::import_edl(text, oma::Rational::literal(30, 1));
    if (imported && !oma::ir::validate(imported->timeline)) {
        __builtin_trap();
    }
    return 0;
}
