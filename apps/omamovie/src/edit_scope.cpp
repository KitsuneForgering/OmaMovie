#include "edit_scope.hpp"

#include <QVariantList>

#include <unordered_map>

namespace tl = oma::timeline;

QVariantMap edit_scope(const tl::Timeline& before, const tl::Timeline& after) {
    struct Span {
        std::int64_t start;
        std::int64_t end;
    };
    std::unordered_map<std::uint64_t, Span> now;
    for (const tl::Track& track : after.tracks()) {
        for (const tl::Clip& c : track.clips)
            now.emplace(c.id.value(), Span{c.start_ticks(), c.end_ticks()});
    }
    QVariantMap moved;
    QVariantList removed;
    QVariantList cut;
    for (const tl::Track& track : before.tracks()) {
        for (const tl::Clip& c : track.clips) {
            const auto it = now.find(c.id.value());
            const auto id = static_cast<double>(c.id.value());
            if (it == now.end()) {
                removed.push_back(id);
                continue;
            }
            const Span s = it->second;
            if (s.start != c.start_ticks())
                moved.insert(QString::number(c.id.value()), s.start < c.start_ticks() ? -1 : 1);
            if (s.end - s.start < c.end_ticks() - c.start_ticks())
                cut.push_back(id);
        }
    }
    int markers = 0;
    for (const tl::Marker& m : before.markers()) {
        const tl::Marker* moved_to = after.find_marker(m.id);
        markers += moved_to == nullptr || moved_to->time != m.time ? 1 : 0;
    }
    return {{"active", true},
            {"moved", moved},
            {"removed", removed},
            {"cut", cut},
            {"markers", markers}};
}
