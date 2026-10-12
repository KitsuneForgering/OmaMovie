#include "export_requirements.hpp"

std::optional<oma::timeline::LutId> missing_export_lut(const oma::timeline::Timeline& timeline,
                                                       const LutTables& loaded) {
    for (const auto& track : timeline.tracks()) {
        if (track.kind != oma::timeline::TrackKind::Video || track.hidden)
            continue;
        for (const auto& clip : track.clips) {
            const auto& grade = clip.video.grade;
            if (grade.lut.valid() && grade.lut_amount > 0 && !loaded.contains(grade.lut.value()))
                return grade.lut;
        }
    }
    return std::nullopt;
}
