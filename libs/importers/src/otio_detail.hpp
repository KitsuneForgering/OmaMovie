#pragma once

// Shared pieces of the OTIO reader: the parse context and the time and object helpers.

#include "oma/importers/otio.hpp"

#include <simdjson.h>

#include <string>

namespace oma::importers::otio {

using Element = simdjson::dom::element;
using Object = simdjson::dom::object;

inline constexpr std::size_t kMaxItems = 100000;

struct Context {
    ir::ImportReport& report;
    std::size_t items = 0;
    void issue(std::string object, std::string reason, std::string impact,
               ir::SupportLevel level = ir::SupportLevel::Recognized);
};

// The object's OTIO_SCHEMA, or "" when it has none.
[[nodiscard]] std::string schema_of(Object o);
// A string member, or "" when missing or not a string.
[[nodiscard]] std::string text(Object o, std::string_view key);
// The element as an opaque IR object (minified JSON kept as the raw payload).
[[nodiscard]] ir::Opaque opaque(Element e, std::string_view id);

// An OTIO rate as an exact fraction; one without an exact fraction is reported.
[[nodiscard]] Result<Rational> rate_of(double rate, Context& c, std::string_view where);
// RationalTime.1 and TimeRange.1, converted to exact times; rounding is reported as a loss.
[[nodiscard]] Result<RationalTime> time_of(Element e, Context& c, std::string_view where);
[[nodiscard]] Result<TimeRange> range_of(Element e, Context& c, std::string_view where);

// The clip's active media reference as an IR media id (reusing one with the same target), with
// its available range; references without a file are reported.
[[nodiscard]] Result<std::string> media_of(Object clip, ir::Timeline& tl, Context& c);
// A Clip.1/Clip.2 object: its media, source range and, kept as opaque, its effects and markers.
[[nodiscard]] Result<ir::Item> clip_of(Object o, const std::string& name, ir::Timeline& tl,
                                       Context& c);
// One child of a track: a clip, gap or transition, or nullopt when the caller should keep it
// as an opaque object (already reported).
[[nodiscard]] Result<std::optional<ir::Item>> item_of(Object o, ir::Timeline& tl, Context& c);

// The top-level Stack.1 under `tracks`: its Track.1 children as IR tracks, anything else kept
// as opaque; then IR validation. Completes `out`, whose report `c` writes to.
[[nodiscard]] Result<void> read_stack(Object top, Imported& out, Context& c);

} // namespace oma::importers::otio
