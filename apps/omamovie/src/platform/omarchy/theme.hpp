#pragma once

// QObject/QString first: GCC 16 rejects QChar's definition after QByteArray's headers probed it
// as incomplete (-Wsfinae-incomplete).
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QFileSystemWatcher>
#include <QFont>
#include <QProcess>
#include <QTimer>
#include <QVariantMap>

// The active Omarchy palette and font (Docs/Research/omarchy-integration.md §3), the one place
// the app reads them. Both come from the installed scripts (`omarchy-theme-color --all`,
// `omarchy-font-current`), run asynchronously so the UI thread never waits on them, and are
// re-read when the theme state or fontconfig changes.
//
// `omarchy-theme-set` deletes the current theme directory before moving the next one in, so
// a change is debounced and a read that fails or yields no colour keeps the last good palette.
// Outside Omarchy (no scripts, no state directory) the built-in palette and the system's
// monospace font stay.
namespace omarchy {

// The palette keys the UI uses, each with its fallback value (a dark neutral palette).
[[nodiscard]] QVariantMap fallback_palette();

// `omarchy-theme-color --all` output (key<TAB>value lines) applied over `base`: only known keys
// with a #rgb/#rrggbb value (or `mode`), so a partial or malformed palette changes nothing
// else. `accepted` reports whether any value was accepted.
[[nodiscard]] QVariantMap merge_palette(const QVariantMap& base, const QByteArray& output, bool* accepted);

class Theme : public QObject {
    Q_OBJECT
public:
    // `state_dir` is ~/.local/state/omarchy/current and `fontconfig_dir` ~/.config/fontconfig
    // in normal runs; the smoke run passes its own.
    Theme(QString state_dir, QString fontconfig_dir, QObject* parent = nullptr);

    [[nodiscard]] const QVariantMap& palette() const { return palette_; }
    [[nodiscard]] const QFont& font() const { return font_; }

    // Reads both now (asynchronously); also what a watched change triggers after the debounce.
    void refresh();
    // Startup only: reads both and waits (bounded by the script timeout) so the first frame
    // already has the theme.
    void load();

signals:
    void paletteChanged();
    void fontChanged();

private:
    void watch();
    void read_palette();
    void read_font();

    QString state_dir_;
    QString fontconfig_dir_;
    QVariantMap palette_;
    QFont font_;
    QFileSystemWatcher watcher_;
    QTimer debounce_;
    QProcess palette_process_;
    QProcess font_process_;
};

} // namespace omarchy
