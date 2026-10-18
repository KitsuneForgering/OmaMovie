#pragma once

// QString before QDateTime: GCC 16 rejects QChar's definition after it was probed incomplete.
#include <QString>
#include <QDateTime>

#include <filesystem>
#include <vector>

// Autosave files (CLAUDE.md §14: separate from the main file). A copy of the project in the
// user's state folder, written while there are unsaved changes, with a sidecar naming the
// project it came from. Saving or discarding removes it; one left behind after a crash is
// offered for recovery on the Projects screen.
namespace autosave {

// $XDG_STATE_HOME/omamovie/autosave, else ~/.local/state/omamovie/autosave.
[[nodiscard]] std::filesystem::path dir();

// The sidecar holding the original project path ("" for an untitled project).
[[nodiscard]] std::filesystem::path origin_of(const std::filesystem::path& file);

struct Recovered {
    std::filesystem::path file;
    QString origin; // the project it autosaved, empty if it was never saved
    QDateTime when;
};

// Autosaves in `folder`, newest first.
[[nodiscard]] std::vector<Recovered> list(const std::filesystem::path& folder);

// Removes an autosave and its sidecar; missing files are fine.
void remove(const std::filesystem::path& file);

} // namespace autosave

// Prior versions of a project (M7: recovering from a mistaken edit after saving). Before a save
// replaces a project file, the old file is copied to the state folder; the newest kKeep stay.
// Source media are never copied: a version holds the project, not its footage.
namespace versions {

inline constexpr std::size_t kKeep = 10;

// $XDG_STATE_HOME/omamovie/versions/<name>-<hash of the path>.
[[nodiscard]] std::filesystem::path dir_for(const QString& project);

// Copies `project` (if it exists) into its versions folder and trims the oldest. Returns false
// when the copy failed; the save goes on regardless.
bool archive(const QString& project);

// Versions of `project`, newest first; `origin` is the project path.
[[nodiscard]] std::vector<autosave::Recovered> list(const QString& project);

} // namespace versions
