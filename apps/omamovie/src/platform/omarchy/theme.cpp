#include "platform/omarchy/theme.hpp"

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>

#include <utility>

namespace omarchy {

namespace {

// Long enough to span theme-set's delete, move and theme.name write.
constexpr int kDebounceMs = 400;
constexpr int kScriptTimeoutMs = 2000;

} // namespace

QVariantMap fallback_palette() {
    return {{"background", "#111318"},      {"dark_background", "#171a20"},
            {"darker_background", "#0b0d11"}, {"lighter_background", "#262a33"},
            {"foreground", "#e2e5ee"},      {"dark_foreground", "#8b91a2"},
            {"accent", "#8ea2ff"},          {"selection_background", "#26314a"},
            {"blue", "#4f8dff"},            {"green", "#4dffa6"},
            {"yellow", "#ffd75f"},          {"red", "#ff6b6b"},
            {"mode", "dark"}};
}

QVariantMap merge_palette(const QVariantMap& base, const QByteArray& output, bool* accepted) {
    QVariantMap palette = base;
    bool any = false;
    for (const QByteArray& line : output.split('\n')) {
        const auto parts = line.split('\t');
        if (parts.size() != 2) continue;
        const QString key = QString::fromUtf8(parts[0]).trimmed();
        const QString value = QString::fromUtf8(parts[1]).trimmed();
        if (!palette.contains(key)) continue;
        const bool valid = key == QLatin1String("mode")
                               ? value == QLatin1String("dark") || value == QLatin1String("light")
                               : value.startsWith(QLatin1Char('#')) && QColor::isValidColorName(value);
        if (!valid) continue;
        palette.insert(key, value);
        any = true;
    }
    if (accepted != nullptr) *accepted = any;
    return palette;
}

Theme::Theme(QString state_dir, QString fontconfig_dir, QObject* parent)
    : QObject(parent), state_dir_(std::move(state_dir)), fontconfig_dir_(std::move(fontconfig_dir)),
      palette_(fallback_palette()), font_(QFontDatabase::systemFont(QFontDatabase::FixedFont)) {
    debounce_.setSingleShot(true);
    debounce_.setInterval(kDebounceMs);
    connect(&debounce_, &QTimer::timeout, this, &Theme::refresh);
    const auto changed = [this](const QString&) { debounce_.start(); };
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, changed);
    connect(&watcher_, &QFileSystemWatcher::fileChanged, this, changed);

    connect(&palette_process_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (status != QProcess::NormalExit || code != 0) return; // keep the last good palette
        bool accepted = false;
        QVariantMap next = merge_palette(fallback_palette(), palette_process_.readAllStandardOutput(), &accepted);
        if (!accepted || next == palette_) return;
        palette_ = std::move(next);
        emit paletteChanged();
    });
    connect(&font_process_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (status != QProcess::NormalExit || code != 0) return;
        const QString family = QString::fromUtf8(font_process_.readAllStandardOutput()).trimmed();
        if (family.isEmpty() || family == font_.family()) return;
        font_ = QFont(family);
        emit fontChanged();
    });
    // A script that hangs must not keep the next change from being read.
    for (QProcess* process : {&palette_process_, &font_process_}) {
        connect(process, &QProcess::started, this, [process] {
            QTimer::singleShot(kScriptTimeoutMs, process, [process] {
                if (process->state() != QProcess::NotRunning) process->kill();
            });
        });
    }
    watch();
}

void Theme::watch() {
    // theme-set replaces files and directories, which drops their watches: re-add every time.
    // The parent directories are watched too, so a path that is missing now is picked up once
    // it appears.
    const QStringList wanted{state_dir_, state_dir_ + QStringLiteral("/theme"),
                             state_dir_ + QStringLiteral("/theme.name"), fontconfig_dir_,
                             fontconfig_dir_ + QStringLiteral("/fonts.conf")};
    for (const QString& path : wanted) {
        if (QFileInfo::exists(path) && !watcher_.files().contains(path) && !watcher_.directories().contains(path)) {
            watcher_.addPath(path);
        }
    }
}

void Theme::refresh() {
    watch();
    read_palette();
    read_font();
}

void Theme::load() {
    refresh();
    palette_process_.waitForFinished(kScriptTimeoutMs);
    font_process_.waitForFinished(kScriptTimeoutMs);
}

void Theme::read_palette() {
    if (palette_process_.state() != QProcess::NotRunning) {
        debounce_.start(); // still reading the previous change: read again once it is done
        return;
    }
    palette_process_.start(QStringLiteral("omarchy-theme-color"), {QStringLiteral("--all")});
}

void Theme::read_font() {
    if (font_process_.state() != QProcess::NotRunning) {
        debounce_.start();
        return;
    }
    font_process_.start(QStringLiteral("omarchy-font-current"), {});
}

} // namespace omarchy
