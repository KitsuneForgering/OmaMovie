#include "oma/importers/timeline.hpp"

#include "oma/importers/edl.hpp"
#include "oma/importers/mlt.hpp"

namespace oma::importers {

Result<Imported> import_timeline(std::string_view text, std::string_view extension,
                                 Rational edl_rate) {
    if (extension == ".edl")
        return import_edl(text, edl_rate);
    if (extension == ".mlt" || extension == ".kdenlive")
        return import_mlt(text);
    return import_otio(text);
}

} // namespace oma::importers
