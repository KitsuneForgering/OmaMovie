#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <utility>

// The Projects screen's recent list (ui-design §3): project files opened or saved, newest
// first, kept in an INI file outside any project. A file that no longer exists is not shown
// (it may come back, e.g. an unmounted disk) and is dropped when the list is next written.
class RecentProjects {
public:
    static constexpr qsizetype kLimit = 12;

    explicit RecentProjects(QString settings_file) : settings_file_(std::move(settings_file)) {}

    // Moves `path` (made absolute) to the front, keeping at most kLimit entries.
    void add(const QString& path);
    void remove(const QString& path);
    // Rows {path, name, folder, when} of the files that exist.
    [[nodiscard]] QVariantList rows() const;

private:
    [[nodiscard]] QStringList read() const;
    void write(const QStringList& paths) const;

    QString settings_file_;
};
