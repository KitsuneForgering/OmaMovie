#include "caption_io.hpp"

#include <QSaveFile>

#include <algorithm>
#include <cstdint>

namespace caption_io {
std::vector<oma::project::Cue> cues(const oma::timeline::Timeline& timeline) {
    std::vector<oma::project::Cue> out;
    out.reserve(timeline.captions().size());
    const auto ms = [](const oma::RationalTime& time) {
        return oma::rescale(time.value(), time.timebase(), oma::Rational::literal(1, 1000),
                            oma::Rounding::Nearest)
            .value_or(0);
    };
    for (const auto& caption : timeline.captions())
        out.push_back(
            {.start_ms = ms(caption.start),
             .end_ms = ms(caption.start) + std::max<std::int64_t>(1, ms(caption.duration)),
             .text = caption.text});
    return out;
}

bool write(const QString& path, std::span<const oma::project::Cue> cues) {
    const std::string contents = path.endsWith(QStringLiteral(".vtt"), Qt::CaseInsensitive)
                                     ? oma::project::write_vtt(cues)
                                     : oma::project::write_srt(cues);
    QSaveFile out(path);
    return out.open(QIODevice::WriteOnly) &&
           out.write(contents.data(), static_cast<qint64>(contents.size())) ==
               static_cast<qint64>(contents.size()) &&
           out.commit();
}
} // namespace caption_io
