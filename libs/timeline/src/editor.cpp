#include "oma/timeline/editor.hpp"

#include "anchors.hpp"
#include "mutation.hpp"

#include <format>

namespace oma::timeline {

Result<void> Editor::execute(std::unique_ptr<Command> command) {
    if (!command) {
        return detail::error(ErrorCode::InvalidArgument, "no command");
    }
    // Connected clips follow their primaries (ADR-0014): their moves join this edit's undo entry.
    const bool anchored = detail::has_anchors(timeline_);
    const detail::Places before = anchored ? detail::places(timeline_) : detail::Places{};
    if (auto r = command->apply(timeline_); !r) {
        return r;
    }
    if (anchored) {
        auto fixes = detail::reattach(timeline_, before);
        if (!fixes) {
            command->revert(timeline_);
            return std::unexpected(fixes.error());
        }
        if (!fixes->empty()) {
            auto follow = detail::make_transaction("Follow connections", std::move(*fixes));
            if (auto r = follow->apply(timeline_); !r) {
                command->revert(timeline_);
                return r;
            }
            const std::string name(command->name());
            detail::Steps both;
            both.push_back(std::move(command));
            both.push_back(std::move(follow));
            command = detail::make_transaction(name, std::move(both)); // both already applied
        }
    }
    // Commands check their local preconditions (overlaps, edges); the timeline-wide rules
    // (media kinds, source ranges, properties) are checked once here. Rejecting an edit is a
    // normal outcome reported to the caller, not a fault, so it is not logged.
    if (auto valid = timeline_.validate(); !valid) {
        command->revert(timeline_);
        return valid;
    }
    undo_.push_back(std::move(command));
    redo_.clear();
    ++revision_;
    return {};
}

Result<void> Editor::add_media(MediaInfo media) {
    if (!media.id.valid() || timeline_.find_media(media.id) != nullptr) {
        return detail::error(ErrorCode::InvalidArgument, "media ID is invalid or in use",
                             std::format("media {}", media.id.value()));
    }
    if (!media.still &&
        (media.duration.value() < 0 || media.start.timebase() != media.duration.timebase())) {
        return detail::error(ErrorCode::InvalidArgument, "invalid media range",
                             std::format("media {}", media.id.value()));
    }
    detail::Mutation(timeline_).media().push_back(media);
    ++revision_;
    return {};
}

Result<void> Editor::add_lut(LutInfo lut) {
    if (!lut.id.valid() || timeline_.find_lut(lut.id) != nullptr) {
        return detail::error(ErrorCode::InvalidArgument, "LUT ID is invalid or in use",
                             std::format("LUT {}", lut.id.value()));
    }
    detail::Mutation(timeline_).luts().push_back(std::move(lut));
    ++revision_;
    return {};
}

std::string_view Editor::undo_name() const noexcept {
    return undo_.empty() ? std::string_view{} : undo_.back()->name();
}

std::string_view Editor::redo_name() const noexcept {
    return redo_.empty() ? std::string_view{} : redo_.back()->name();
}

Result<void> Editor::undo() {
    if (undo_.empty()) {
        return detail::error(ErrorCode::InvalidArgument, "nothing to undo");
    }
    undo_.back()->revert(timeline_);
    redo_.push_back(std::move(undo_.back()));
    undo_.pop_back();
    ++revision_;
    return {};
}

Result<void> Editor::redo() {
    if (redo_.empty()) {
        return detail::error(ErrorCode::InvalidArgument, "nothing to redo");
    }
    // The state is the one the command first applied to, so this succeeds unless the command
    // is not deterministic; then it stays on the redo stack and the timeline is unchanged.
    if (auto r = redo_.back()->apply(timeline_); !r) {
        return r;
    }
    undo_.push_back(std::move(redo_.back()));
    redo_.pop_back();
    ++revision_;
    return {};
}

void Editor::clear_history() noexcept {
    undo_.clear();
    redo_.clear();
    ++revision_;
}

TrackId Editor::new_track_id() noexcept {
    return TrackId(detail::Mutation(timeline_).allocate_id());
}

ClipId Editor::new_clip_id() noexcept {
    return ClipId(detail::Mutation(timeline_).allocate_id());
}

MarkerId Editor::new_marker_id() noexcept {
    return MarkerId(detail::Mutation(timeline_).allocate_id());
}

} // namespace oma::timeline
