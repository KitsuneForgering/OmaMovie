// libFuzzer target for the .cube parser (CLAUDE.md §18): any input must give a LUT that passes
// validation or a handled error, never a crash, UB or unbounded work. Run with `make fuzz`.

#include "oma/compositor/grade.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    auto lut = oma::compositor::parse_cube(text);
    if (lut) {
        oma::compositor::Grade grade;
        grade.lut = std::make_shared<const oma::compositor::Lut3d>(std::move(*lut));
        if (!oma::compositor::validate(grade)) {
            __builtin_trap(); // the parser accepted a table the compositor would refuse
        }
    }
    return 0;
}
