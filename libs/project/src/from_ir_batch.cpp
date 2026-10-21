#include "from_ir_detail.hpp"

#include "oma/timeline/edit.hpp"

#include <algorithm>

namespace oma::project::from_ir_detail {

namespace tl = oma::timeline;

void issue(ir::ImportReport& report, std::string object, std::string reason, std::string impact) {
    report.issues.push_back({.object = std::move(object),
                             .reason = std::move(reason),
                             .level = ir::SupportLevel::Edits,
                             .impact = std::move(impact)});
}

void execute_each(tl::Editor& editor, std::span<const Edit> edits, std::span<std::uint8_t> landed) {
    if (edits.empty())
        return;
    if (edits.size() == 1) {
        landed[0] = editor.execute(edits[0]()).has_value() ? 1 : 0;
        return;
    }
    std::vector<std::unique_ptr<tl::Command>> group;
    group.reserve(edits.size());
    for (const Edit& make : edits)
        group.push_back(make());
    if (editor.execute(tl::edit::transaction("Import", std::move(group)))) {
        std::ranges::fill(landed, std::uint8_t{1});
        return;
    }
    const std::size_t half = edits.size() / 2;
    execute_each(editor, edits.first(half), landed.first(half));
    execute_each(editor, edits.subspan(half), landed.subspan(half));
}

} // namespace oma::project::from_ir_detail
