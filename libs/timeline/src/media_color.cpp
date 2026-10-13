// edit::set_media_color (ADR-0019): a media item's color interpretation, one undoable step.
#include "oma/timeline/edit.hpp"

#include "mutation.hpp"

#include <algorithm>
#include <format>
#include <utility>

namespace oma::timeline::edit {

namespace {
class SetMediaColor final : public Command {
public:
    SetMediaColor(MediaId id, MediaColorMatrix matrix, MediaColorRange range)
        : id_(id), matrix_(matrix), range_(range) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "Color Interpretation"; }

    [[nodiscard]] Result<void> apply(Timeline& timeline) override {
        MediaInfo* m = find(timeline);
        if (m == nullptr)
            return detail::error(ErrorCode::InvalidArgument, "unknown media",
                                 std::format("media {}", id_.value()));
        old_matrix_ = std::exchange(m->color_matrix, matrix_);
        old_range_ = std::exchange(m->color_range, range_);
        return {};
    }

    void revert(Timeline& timeline) noexcept override {
        if (MediaInfo* m = find(timeline); m != nullptr) {
            m->color_matrix = old_matrix_;
            m->color_range = old_range_;
        }
    }

private:
    MediaInfo* find(Timeline& timeline) {
        auto& media = detail::Mutation(timeline).media();
        const auto it = std::ranges::find(media, id_, &MediaInfo::id);
        return it == media.end() ? nullptr : &*it;
    }

    MediaId id_;
    MediaColorMatrix matrix_;
    MediaColorRange range_;
    MediaColorMatrix old_matrix_ = MediaColorMatrix::Auto;
    MediaColorRange old_range_ = MediaColorRange::Auto;
};
} // namespace

std::unique_ptr<Command> set_media_color(MediaId id, MediaColorMatrix matrix,
                                         MediaColorRange range) {
    return std::make_unique<SetMediaColor>(id, matrix, range);
}

} // namespace oma::timeline::edit
