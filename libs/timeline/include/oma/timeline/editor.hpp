#pragma once

#include "oma/base/error.hpp"
#include "oma/timeline/ids.hpp"
#include "oma/timeline/model.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

// Commands and history (CLAUDE.md §10): every edit is a Command applied through
// Editor::execute, the single entry point. Undo/redo restores exact prior states; compound
// operations are one history entry.
//
// Threading: an Editor and its Timeline belong to one thread (the UI thread, ADR-0003).
// Readers on other threads get copies, never references.

namespace oma::timeline {

class Command {
public:
    Command() = default;
    Command(const Command&) = delete;
    Command& operator=(const Command&) = delete;
    Command(Command&&) = delete;
    Command& operator=(Command&&) = delete;
    virtual ~Command() = default;

    // Shown in the history ("Undo Split").
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    // Applies the edit. On failure the timeline is left exactly as it was.
    [[nodiscard]] virtual Result<void> apply(Timeline& timeline) = 0;
    // Undoes a successful apply. Called only in reverse order of application, so the state it
    // finds is the one its apply left behind.
    virtual void revert(Timeline& timeline) noexcept = 0;
};

class Editor {
public:
    explicit Editor(Timeline timeline) noexcept : timeline_(std::move(timeline)) {}

    [[nodiscard]] const Timeline& timeline() const noexcept { return timeline_; }

    // Registers media for clips to reference. Importing is not a timeline edit: it stays out
    // of the history (undoing an edit never unregisters media a library still shows), and
    // registered media is never removed while the timeline lives.
    [[nodiscard]] Result<void> add_media(MediaInfo media);

    // Applies a command, checks every timeline invariant and records it for undo; clears the
    // redo stack. A failing command or a broken invariant leaves the timeline unchanged.
    [[nodiscard]] Result<void> execute(std::unique_ptr<Command> command);

    [[nodiscard]] bool can_undo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !redo_.empty(); }
    [[nodiscard]] std::string_view undo_name() const noexcept;
    [[nodiscard]] std::string_view redo_name() const noexcept;
    [[nodiscard]] Result<void> undo();
    [[nodiscard]] Result<void> redo();
    // Forgets undo and redo, keeping the timeline: after building a new project's initial
    // tracks or loading one, so the first undo is the user's first edit.
    void clear_history() noexcept;

    // Fresh IDs for objects a caller is about to create (tracks, clips, markers), so the
    // caller knows them before executing the command. IDs are never reused, even after undo.
    [[nodiscard]] TrackId new_track_id() noexcept;
    [[nodiscard]] ClipId new_clip_id() noexcept;
    [[nodiscard]] MarkerId new_marker_id() noexcept;

    // Increases on every change (execute, undo, redo), so views know when to refresh.
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }

private:
    Timeline timeline_;
    std::vector<std::unique_ptr<Command>> undo_;
    std::vector<std::unique_ptr<Command>> redo_;
    std::uint64_t revision_ = 0;
};

} // namespace oma::timeline
