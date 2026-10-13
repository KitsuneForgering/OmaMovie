#include "property_run.hpp"

namespace property {

namespace {

bool fails_alike(const std::vector<Op>& log, const std::string& kind) {
    const Run run = replay(log);
    return run.failure && run.failure->kind == kind;
}

} // namespace

std::vector<Op> shrink(std::vector<Op> log, const std::string& kind) {
    // Remove spans, halving their size (delta debugging), while the failure stays.
    for (std::size_t span = log.size() / 2; span >= 1; span /= 2) {
        for (std::size_t at = 0; at + span <= log.size();) {
            std::vector<Op> shorter(log.begin(), log.begin() + static_cast<std::ptrdiff_t>(at));
            shorter.insert(shorter.end(), log.begin() + static_cast<std::ptrdiff_t>(at + span),
                           log.end());
            if (fails_alike(shorter, kind))
                log = std::move(shorter);
            else
                at += span;
        }
    }
    // Then bring each value toward zero.
    for (Op& op : log)
        for (std::int64_t* v : {&op.a, &op.b, &op.c})
            for (const std::int64_t smaller : {std::int64_t{0}, *v / 2})
                if (smaller != *v) {
                    const std::int64_t was = *v;
                    *v = smaller;
                    if (!fails_alike(log, kind))
                        *v = was;
                }
    return log;
}

} // namespace property
