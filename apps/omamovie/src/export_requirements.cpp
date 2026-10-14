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

std::optional<oma::timeline::UnknownEffect>
unrendered_export_effect(const oma::timeline::Timeline& timeline) {
    for (const auto& track : timeline.tracks()) {
        if (track.kind != oma::timeline::TrackKind::Video || track.hidden)
            continue;
        for (const auto& clip : track.clips) {
            for (const auto& e : clip.video.effects) {
                if (e.enabled && oma::timeline::find_effect_definition(e.definition) == nullptr)
                    return oma::timeline::UnknownEffect{.clip = clip.id,
                                                        .definition = e.definition};
            }
        }
    }
    return std::nullopt;
}
