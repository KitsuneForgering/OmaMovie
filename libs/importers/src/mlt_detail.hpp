#pragma once

// The MLT document as the importer sees it (mlt_*.cpp): elements indexed by id.

#include "mlt_model.hpp"
#include "xml.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace oma::importers::mlt {

// `5`, `00:00:01.500` (clock) or `00:00:01:15` (timecode) as frames; none otherwise.
[[nodiscard]] std::optional<std::int64_t> frames_of(std::string_view value, const Document& doc);

// Indexes the root's producers, chains, playlists and tractors; reports what it skips.
[[nodiscard]] Result<Document> index(const xml::Element& root, ir::ImportReport& report);

// The IR tracks of the main tractor (a track that is a tractor contributes its own tracks).
[[nodiscard]] Result<void> build(const Document& doc, Imported& out);

// The tracks being built, with media shared by file (mlt_build.cpp, mlt_items.cpp).
struct Builder {
    const Document& doc;
    Imported& out;
    std::map<std::string, std::string> media; // resource -> media id
};

// `playlist`'s entries as IR items on `track`.
void items_of(Builder& b, const Playlist& playlist, ir::Track& track);

[[nodiscard]] std::string property(const xml::Element& e, std::string_view name);
[[nodiscard]] Tractor tractor_of(const xml::Element& e, const Document& doc);
[[nodiscard]] Producer producer_of(const xml::Element& e, const Document& doc,
                                   std::string_view root);
[[nodiscard]] Playlist playlist_of(const xml::Element& e, const Document& doc);
// A media clip of `p` from entry point `in` to `out`; none for a generator (colour, black).
[[nodiscard]] std::optional<ir::Clip> clip_of(Builder& b, const Producer& p, std::int64_t in,
                                              std::int64_t out);
// `frames` of the profile rate; with `speed`, the source time of that many timewarp frames.
[[nodiscard]] RationalTime at(const Builder& b, std::int64_t frames,
                              Rational speed = Rational::literal(1, 1));
// A timewarp's warp_speed, exactly; none when it is not a decimal number.
[[nodiscard]] std::optional<Rational> speed_of(std::string_view value);
// Why `id` cannot be a clip or track: a colour/black generator, a nested sequence, or unknown.
[[nodiscard]] std::string why_not(const Builder& b, const std::string& id);
[[nodiscard]] ir::Opaque opaque(const xml::Element& e);
// Whether `o` was kept from a `<transition>` or a `<filter>` (opaque records the element).
[[nodiscard]] bool is_a(const ir::Opaque& o, std::string_view element);
// A filter that cannot change the output: disabled (`disable=1`, as Kdenlive adds volume and
// panner to every track) or a level meter (audiolevel). Kept, but not reported.
[[nodiscard]] bool inert(const ir::Opaque& o);
void issue(ir::ImportReport& r, std::string object, std::string reason, std::string impact);

} // namespace oma::importers::mlt
