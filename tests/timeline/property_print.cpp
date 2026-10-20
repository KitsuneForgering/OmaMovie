#include "property_ops.hpp"

#include <array>
#include <format>

namespace property {

std::string to_string(const Op& op) {
    static constexpr std::array<const char*, kKinds> kNames{
        "Append",       "Insert",        "Overwrite", "Split",     "TrimStart",    "TrimEnd",
        "RippleDelete", "Remove",        "Roll",      "Slip",      "Slide",        "Move",
        "RippleMove",   "Speed",         "Reverse",   "Freeze",    "Connect",      "Disconnect",
        "Ramp",         "Transition",    "Detach",    "AddMarker", "RemoveMarker", "AddCaption",
        "SetCaption",   "RemoveCaption", "Video",     "Undo",      "Redo"};
    return std::format("{{Kind::{}, {}, {}, {}, {}, {}}}",
                       kNames.at(static_cast<std::size_t>(op.kind)), op.track, op.clip, op.a, op.b,
                       op.c);
}

} // namespace property
