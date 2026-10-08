#include "commands.hpp"

#include "mutation.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <ranges>
#include <utility>

namespace oma::timeline::detail {

namespace {

void sort_clips(Track& t) {
    std::ranges::sort(t.clips, {}, &Clip::start_ticks);
}

// Whether [start, end) is free on the track, ignoring one clip (the one being replaced).
bool range_free(const Track& t, std::int64_t start, std::int64_t end, ClipId ignore = {}) {
    return std::ranges::none_of(t.clips, [&](const Clip& c) {
        return c.id != ignore && c.start_ticks() < end && start < c.end_ticks();
    });
}

Result<std::int64_t> clip_end(const Clip& c) {
    return add_ticks(c.start.value(), c.duration.value());
}

struct ClipLocation {
    Track* track = nullptr;
    std::size_t index = 0;
};

std::optional<ClipLocation> locate(Mutation& m, ClipId id) {
    for (Track& t : m.tracks()) {
        const auto it = std::ranges::find(t.clips, id, &Clip::id);
        if (it != t.clips.end()) {
            return ClipLocation{.track = &t,
                                .index = static_cast<std::size_t>(it - t.clips.begin())};
        }
    }
    return std::nullopt;
}

class Transaction final : public Command {
public:
    Transaction(std::string name, Steps steps) : name_(std::move(name)), steps_(std::move(steps)) {}

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

    [[nodiscard]] Result<void> apply(Timeline& timeline) override {
        for (std::size_t i = 0; i < steps_.size(); ++i) {
            if (auto r = steps_[i]->apply(timeline); !r) {
                for (std::size_t j = i; j > 0; --j) {
                    steps_[j - 1]->revert(timeline);
                }
                return r;
            }
        }
        return {};
    }

    void revert(Timeline& timeline) noexcept override {
        for (auto& step : std::views::reverse(steps_)) {
            step->revert(timeline);
        }
    }

private:
    std::string name_;
    Steps steps_;
};

class Planned final : public Command {
public:
    Planned(std::string name, Planner planner)
        : name_(std::move(name)), planner_(std::move(planner)) {}

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

    [[nodiscard]] Result<void> apply(Timeline& timeline) override {
        if (!plan_) {
            auto steps = planner_(timeline);
            if (!steps) {
                return std::unexpected(steps.error());
            }
            plan_ = make_transaction(name_, std::move(*steps));
        }
        auto r = plan_->apply(timeline);
        if (!r) {
            plan_.reset(); // the next attempt plans again from the state it finds
        }
        return r;
    }

    void revert(Timeline& timeline) noexcept override { plan_->revert(timeline); }

private:
    std::string name_;
    Planner planner_;
    std::unique_ptr<Command> plan_;
};

class InsertClip final : public Command {
public:
    InsertClip(TrackId track, Clip clip) : track_(track), clip_(std::move(clip)) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "Insert Clip"; }

    [[nodiscard]] Result<void> apply(Timeline& timeline) override {
        Mutation m(timeline);
        Track* t = m.track(track_);
        if (t == nullptr) {
            return error(ErrorCode::InvalidArgument, "unknown track", track_context(track_));
        }
        if (!clip_.id.valid() || timeline.find_clip(clip_.id) != nullptr) {
            return error(ErrorCode::InvalidArgument, "clip ID is invalid or in use",
                         clip_context(clip_.id));
        }
        auto end = clip_end(clip_);
        if (!end) {
            return std::unexpected(end.error());
        }
        if (!range_free(*t, clip_.start.value(), *end)) {
            return error(ErrorCode::InvalidArgument, "the range is occupied",
                         std::format("{} on {}", clip_context(clip_.id), track_context(track_)));
        }
        t->clips.push_back(clip_);
        sort_clips(*t);
        return {};
    }

    void revert(Timeline& timeline) noexcept override {
        Mutation m(timeline);
        if (Track* t = m.track(track_); t != nullptr) {
            std::erase_if(t->clips, [&](const Clip& c) { return c.id == clip_.id; });
        }
    }

private:
    TrackId track_;
    Clip clip_;
};

class EraseClip final : public Command {
public:
    explicit EraseClip(ClipId id) : id_(id) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "Remove Clip"; }

    [[nodiscard]] Result<void> apply(Timeline& timeline) override {
        Mutation m(timeline);
        auto at = locate(m, id_);
        if (!at) {
            return error(ErrorCode::InvalidArgument, "unknown clip", clip_context(id_));
        }
        track_ = at->track->id;
        removed_ = at->track->clips[at->index];
        at->track->clips.erase(at->track->clips.begin() + static_cast<std::ptrdiff_t>(at->index));
        return {};
    }

    void revert(Timeline& timeline) noexcept override {
        Mutation m(timeline);
        if (Track* t = m.track(track_); t != nullptr) {
            t->clips.push_back(removed_);
            sort_clips(*t);
        }
    }

private:
    ClipId id_;
    TrackId track_;
    Clip removed_;
};

class ReplaceClip final : public Command {
public:
    explicit ReplaceClip(Clip clip) : clip_(std::move(clip)) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "Change Clip"; }

    [[nodiscard]] Result<void> apply(Timeline& timeline) override {
        Mutation m(timeline);
        auto at = locate(m, clip_.id);
        if (!at) {
            return error(ErrorCode::InvalidArgument, "unknown clip", clip_context(clip_.id));
        }
        auto end = clip_end(clip_);
        if (!end) {
            return std::unexpected(end.error());
        }
        if (!range_free(*at->track, clip_.start.value(), *end, clip_.id)) {
            return error(ErrorCode::InvalidArgument, "the range is occupied",
                         clip_context(clip_.id));
        }
        old_ = std::exchange(at->track->clips[at->index], clip_);
        sort_clips(*at->track);
        return {};
    }

    void revert(Timeline& timeline) noexcept override {
        Mutation m(timeline);
        if (auto at = locate(m, clip_.id)) {
            at->track->clips[at->index] = old_;
            sort_clips(*at->track);
        }
    }

private:
    Clip clip_;
    Clip old_;
};

class ShiftClips final : public Command {
public:
    ShiftClips(TrackId track, std::int64_t from, std::int64_t delta)
        : track_(track), from_(from), delta_(delta) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "Shift Clips"; }

    [[nodiscard]] Result<void> apply(Timeline& timeline) override {
        Mutation m(timeline);
        Track* t = m.track(track_);
        if (t == nullptr) {
            return error(ErrorCode::InvalidArgument, "unknown track", track_context(track_));
        }
        const auto first =
            std::ranges::find_if(t->clips, [&](const Clip& c) { return c.start_ticks() >= from_; });
        if (first == t->clips.end() || delta_ == 0) {
            shifted_.clear();
            return {};
        }
        // Moving left must not cross time zero or the clips that stay.
        const std::int64_t floor = first == t->clips.begin() ? 0 : std::prev(first)->end_ticks();
        for (auto it = first; it != t->clips.end(); ++it) {
            auto moved = add_ticks(it->start.value(), delta_);
            if (!moved) {
                return std::unexpected(moved.error());
            }
            if (it == first && *moved < floor) {
                return error(ErrorCode::InvalidArgument,
                             "shift collides with an earlier clip or time zero",
                             track_context(track_));
            }
            if (auto end = add_ticks(*moved, it->duration.value()); !end) {
                return std::unexpected(end.error());
            }
        }
        shifted_.clear();
        for (auto it = first; it != t->clips.end(); ++it) {
            it->start = timeline.at(it->start.value() + delta_);
            shifted_.push_back(it->id);
        }
        return {};
    }

    void revert(Timeline& timeline) noexcept override {
        Mutation m(timeline);
        Track* t = m.track(track_);
        if (t == nullptr) {
            return;
        }
        for (Clip& c : t->clips) {
            if (std::ranges::find(shifted_, c.id) != shifted_.end()) {
                c.start = timeline.at(c.start.value() - delta_);
            }
        }
    }

private:
    TrackId track_;
    std::int64_t from_;
    std::int64_t delta_;
    std::vector<ClipId> shifted_;
};

class InsertTrack final : public Command {
public:
    InsertTrack(Track track, std::size_t index) : track_(std::move(track)), index_(index) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "Add Track"; }

    [[nodiscard]] Result<void> apply(Timeline& timeline) override {
        Mutation m(timeline);
        if (!track_.id.valid() || m.track(track_.id) != nullptr) {
            return error(ErrorCode::InvalidArgument, "track ID is invalid or in use",
                         track_context(track_.id));
        }
        if (index_ > m.tracks().size()) {
            return error(ErrorCode::OutOfRange, "track index out of range",
                         track_context(track_.id));
        }
        m.tracks().insert(m.tracks().begin() + static_cast<std::ptrdiff_t>(index_), track_);
        return {};
    }

    void revert(Timeline& timeline) noexcept override {
        Mutation m(timeline);
        std::erase_if(m.tracks(), [&](const Track& t) { return t.id == track_.id; });
    }

private:
    Track track_;
    std::size_t index_;
};

class EraseTrack final : public Command {
public:
    explicit EraseTrack(TrackId id) : id_(id) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "Remove Track"; }

    [[nodiscard]] Result<void> apply(Timeline& timeline) override {
        Mutation m(timeline);
        const auto it = std::ranges::find(m.tracks(), id_, &Track::id);
        if (it == m.tracks().end()) {
            return error(ErrorCode::InvalidArgument, "unknown track", track_context(id_));
        }
        index_ = static_cast<std::size_t>(it - m.tracks().begin());
        removed_ = std::move(*it);
        m.tracks().erase(it);
        return {};
    }

    void revert(Timeline& timeline) noexcept override {
        Mutation m(timeline);
        m.tracks().insert(m.tracks().begin() + static_cast<std::ptrdiff_t>(index_),
                          std::move(removed_));
    }

private:
    TrackId id_;
    std::size_t index_ = 0;
    Track removed_;
};

class SetFlags final : public Command {
public:
    SetFlags(TrackId id, bool muted, bool hidden) : id_(id), muted_(muted), hidden_(hidden) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "Change Track"; }

    [[nodiscard]] Result<void> apply(Timeline& timeline) override {
        Mutation m(timeline);
        Track* t = m.track(id_);
        if (t == nullptr) {
            return error(ErrorCode::InvalidArgument, "unknown track", track_context(id_));
        }
        old_muted_ = std::exchange(t->muted, muted_);
        old_hidden_ = std::exchange(t->hidden, hidden_);
        return {};
    }

    void revert(Timeline& timeline) noexcept override {
        Mutation m(timeline);
        if (Track* t = m.track(id_); t != nullptr) {
            t->muted = old_muted_;
            t->hidden = old_hidden_;
        }
    }

private:
    TrackId id_;
    bool muted_;
    bool hidden_;
    bool old_muted_ = false;
    bool old_hidden_ = false;
};

class InsertMarker final : public Command {
public:
    explicit InsertMarker(Marker marker) : marker_(std::move(marker)) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "Add Marker"; }

    [[nodiscard]] Result<void> apply(Timeline& timeline) override {
        if (!marker_.id.valid() || timeline.find_marker(marker_.id) != nullptr) {
            return error(ErrorCode::InvalidArgument, "marker ID is invalid or in use",
                         std::format("marker {}", marker_.id.value()));
        }
        auto& markers = Mutation(timeline).markers();
        markers.push_back(marker_);
        std::ranges::stable_sort(markers,
                                 [](const Marker& a, const Marker& b) { return a.time < b.time; });
        return {};
    }

    void revert(Timeline& timeline) noexcept override {
        std::erase_if(Mutation(timeline).markers(),
                      [&](const Marker& m) { return m.id == marker_.id; });
    }

private:
    Marker marker_;
};

class EraseMarker final : public Command {
public:
    explicit EraseMarker(MarkerId id) : id_(id) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "Remove Marker"; }

    [[nodiscard]] Result<void> apply(Timeline& timeline) override {
        auto& markers = Mutation(timeline).markers();
        const auto it = std::ranges::find(markers, id_, &Marker::id);
        if (it == markers.end()) {
            return error(ErrorCode::InvalidArgument, "unknown marker",
                         std::format("marker {}", id_.value()));
        }
        index_ = static_cast<std::size_t>(it - markers.begin());
        removed_ = std::move(*it);
        markers.erase(it);
        return {};
    }

    void revert(Timeline& timeline) noexcept override {
        auto& markers = Mutation(timeline).markers();
        markers.insert(markers.begin() + static_cast<std::ptrdiff_t>(index_), std::move(removed_));
    }

private:
    MarkerId id_;
    std::size_t index_ = 0;
    Marker removed_;
};

} // namespace

std::unique_ptr<Command> make_transaction(std::string name, Steps steps) {
    return std::make_unique<Transaction>(std::move(name), std::move(steps));
}

std::unique_ptr<Command> make_planned(std::string name, Planner planner) {
    return std::make_unique<Planned>(std::move(name), std::move(planner));
}

std::unique_ptr<Command> insert_clip(TrackId track, Clip clip) {
    return std::make_unique<InsertClip>(track, std::move(clip));
}

std::unique_ptr<Command> erase_clip(ClipId id) {
    return std::make_unique<EraseClip>(id);
}

std::unique_ptr<Command> replace_clip(Clip clip) {
    return std::make_unique<ReplaceClip>(std::move(clip));
}

std::unique_ptr<Command> shift_clips(TrackId track, std::int64_t from, std::int64_t delta) {
    return std::make_unique<ShiftClips>(track, from, delta);
}

std::unique_ptr<Command> insert_track(Track track, std::size_t index) {
    return std::make_unique<InsertTrack>(std::move(track), index);
}

std::unique_ptr<Command> erase_track(TrackId id) {
    return std::make_unique<EraseTrack>(id);
}

std::unique_ptr<Command> set_flags(TrackId id, bool muted, bool hidden) {
    return std::make_unique<SetFlags>(id, muted, hidden);
}

std::unique_ptr<Command> insert_marker(Marker marker) {
    return std::make_unique<InsertMarker>(std::move(marker));
}

std::unique_ptr<Command> erase_marker(MarkerId id) {
    return std::make_unique<EraseMarker>(id);
}

namespace {

// One step for every caption edit (ADR-0017): the whole list is swapped, so undo is exact.
// O(captions) per edit; at most kMaxCaptions.
class ReplaceCaptions final : public Command {
public:
    explicit ReplaceCaptions(std::vector<Caption> captions) : next_(std::move(captions)) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "Captions"; }

    [[nodiscard]] Result<void> apply(Timeline& timeline) override {
        std::ranges::stable_sort(next_, {}, [](const Caption& c) { return c.start.value(); });
        previous_ = std::exchange(Mutation(timeline).captions(), next_);
        return {};
    }

    void revert(Timeline& timeline) noexcept override { Mutation(timeline).captions() = previous_; }

private:
    std::vector<Caption> next_;
    std::vector<Caption> previous_;
};

} // namespace

std::unique_ptr<Command> replace_captions(std::vector<Caption> captions) {
    return std::make_unique<ReplaceCaptions>(std::move(captions));
}

namespace {

class SetCanvas final : public Command {
public:
    SetCanvas(std::uint32_t width, std::uint32_t height) : width_(width), height_(height) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "Canvas"; }

    [[nodiscard]] Result<void> apply(Timeline& timeline) override {
        previous_ = {timeline.canvas_width(), timeline.canvas_height()};
        Mutation(timeline).set_canvas(width_, height_);
        return {};
    }

    void revert(Timeline& timeline) noexcept override {
        Mutation(timeline).set_canvas(previous_.first, previous_.second);
    }

private:
    std::uint32_t width_;
    std::uint32_t height_;
    std::pair<std::uint32_t, std::uint32_t> previous_{};
};

} // namespace

std::unique_ptr<Command> set_canvas(std::uint32_t width, std::uint32_t height) {
    return std::make_unique<SetCanvas>(width, height);
}

} // namespace oma::timeline::detail
