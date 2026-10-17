#pragma once

// FFmpeg's color fields (AVColorSpace, AVColorPrimaries, AVColorTransferCharacteristic,
// AVColorRange, AVChromaLocation) can hold values outside their enums when a crafted file sets
// them (found by fuzz_probe). Loading such a value as the enum type is undefined behavior, so
// read the stored integer and treat what FFmpeg does not define as unspecified.

#include <cstring>
#include <type_traits>

namespace oma::media::ff {

template <typename Enum>
[[nodiscard]] Enum defined_or(const Enum& field, Enum count, Enum unspecified) {
    static_assert(std::is_enum_v<Enum> && sizeof(Enum) == sizeof(int));
    int raw = 0;
    std::memcpy(&raw, &field, sizeof raw);
    return raw >= 0 && raw < static_cast<int>(count) ? static_cast<Enum>(raw) : unspecified;
}

} // namespace oma::media::ff
