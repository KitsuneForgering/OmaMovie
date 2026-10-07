#include "recent_projects.hpp"

#include <QDateTime>
#include <QFileInfo>
#include <QLocale>
#include <QSettings>
#include <QVariantMap>

namespace {
constexpr auto kKey = "projects/recent";
} // namespace

QStringList RecentProjects::read() const {
    return QSettings(settings_file_, QSettings::IniFormat).value(QLatin1String(kKey)).toStringList();
}

void RecentProjects::write(const QStringList& paths) const {
    QSettings(settings_file_, QSettings::IniFormat).setValue(QLatin1String(kKey), paths);
}

void RecentProjects::add(const QString& path) {
    const QString absolute = QFileInfo(path).absoluteFilePath();
    QStringList paths = read();
    paths.removeAll(absolute);
    paths.prepend(absolute);
    // Gone files are dropped here, not when reading, so a briefly missing disk keeps its entries.
    paths.removeIf([](const QString& p) { return !QFileInfo::exists(p); });
    if (paths.size() > kLimit) paths.resize(kLimit);
    write(paths);
}

void RecentProjects::remove(const QString& path) {
    QStringList paths = read();
    if (paths.removeAll(QFileInfo(path).absoluteFilePath()) > 0) write(paths);
}

QVariantList RecentProjects::rows() const {
    QVariantList rows;
    for (const QString& path : read()) {
        const QFileInfo info(path);
        if (!info.isFile()) continue;
        rows.append(QVariantMap{
            {QStringLiteral("path"), path},
            {QStringLiteral("name"), info.completeBaseName()},
            {QStringLiteral("folder"), info.absolutePath()},
            {QStringLiteral("when"), QLocale().toString(info.lastModified(), QLocale::ShortFormat)}});
    }
    return rows;
}
