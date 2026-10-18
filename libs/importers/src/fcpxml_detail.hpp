#pragma once

// Shared pieces of the FCPXML reader (fcpxml_*.cpp).

#include "oma/importers/fcpxml.hpp"
#include "xml.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace oma::importers::fcpxml {

struct Asset {
    std::string name;
    std::string src;   // file:// URL or path
    std::string start; // FCPXML times as written, read on the sequence's grid when used
    std::string duration;
};

// What the reader keeps while it walks the document.
struct Context {
    Imported& out;
    Rational frame;                           // the sequence's frame duration: the IR's timebase
    std::map<std::string, Rational> formats;  // format id -> frame duration
    std::map<std::string, Asset> assets;      // asset id -> file
    std::map<std::string, std::string> media; // asset id -> IR media id
};

// `0s`, `3600s`, `1001/30000s` as an exact fraction of a second; none otherwise.
[[nodiscard]] std::optional<Rational> seconds(std::string_view text);
// `text` on the context's frame grid; a value off the grid is rounded and reported.
[[nodiscard]] RationalTime on_grid(Context& c, std::string_view text, std::string_view where);

// a + b and a - b; on overflow `a` (only a hostile file gets there; validation then refuses it).
[[nodiscard]] RationalTime add(const RationalTime& a, const RationalTime& b);
[[nodiscard]] RationalTime sub(const RationalTime& a, const RationalTime& b);

// Every `project`, depth first: under `library/event` (1.5+) or `event`; e's first child `name`.
void projects(const xml::Element& e, std::vector<const xml::Element*>& out);
[[nodiscard]] const xml::Element* child_named(const xml::Element& e, std::string_view name);

// Formats and assets of `<resources>`.
void index_resources(Context& c, const xml::Element& resources);
// The spine's items as IR tracks: the spine itself, then one track per lane of connected clips.
void read_spine(Context& c, const xml::Element& spine);

// The IR media of asset `ref`, made on first use with the asset's range as available.
[[nodiscard]] std::optional<std::string> media_of(Context& c, std::string_view ref);
// The IR clip an `asset-clip` or `clip` element plays (its effects kept opaque); none for one
// without a readable asset (reported).
[[nodiscard]] std::optional<ir::Clip> clip_of(Context& c, const xml::Element& e);

[[nodiscard]] ir::Opaque opaque(const xml::Element& e);
void issue(ir::ImportReport& r, std::string object, std::string reason, std::string impact);

} // namespace oma::importers::fcpxml
