#include "mlt_detail.hpp"

namespace oma::importers::mlt {

Result<Document> index(const xml::Element& root, ir::ImportReport& report) {
    Document doc;
    doc.rate = Rational::literal(25, 1);
    bool profiled = false;
    for (const xml::Element& e : root.children) {
        const std::string id(e.attribute("id"));
        if (e.name == "profile") {
            const auto rate =
                Rational::make(frames_of(e.attribute("frame_rate_num"), doc).value_or(0),
                               frames_of(e.attribute("frame_rate_den"), doc).value_or(0));
            if (!rate || !rate->is_positive() || rate->num() > 1000 * rate->den())
                return make_error(ErrorCode::InvalidData, Category::Importer,
                                  "invalid MLT profile rate");
            doc.rate = *rate;
            doc.fps = (rate->num() + (rate->den() / 2)) / rate->den();
            profiled = true;
        } else if (e.name == "producer" || e.name == "chain") {
            doc.producers[id] = producer_of(e, doc, root.attribute("root"));
        } else if (e.name == "playlist") {
            doc.playlists[id] = playlist_of(e, doc);
        } else if (e.name == "tractor") {
            doc.tractors[id] = tractor_of(e, doc);
            doc.main = id;
        } else if (e.name != "consumer") {
            issue(report, id.empty() ? e.name : id, e.name + " not imported", "left out");
        }
    }
    if (!profiled)
        issue(report, "profile", "no frame rate in the document", "read at 25 fps");
    return doc;
}

} // namespace oma::importers::mlt
