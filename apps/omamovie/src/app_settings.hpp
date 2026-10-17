#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

// User-facing defaults for the Settings screen (implementation plan, M6), separate from any
// project: stored in an INI file under $XDG_CONFIG_HOME/omamovie, each resettable. Values are
// validated on read, so a hand-edited or older file falls back to the default instead of
// reaching the editor. Applying them is the app's job (main.cpp); this class only keeps them.
class AppSettings : public QObject {
    Q_OBJECT
    // "auto", "full", "half", "quarter".
    Q_PROPERTY(QString previewQuality READ previewQuality WRITE setPreviewQuality NOTIFY changed)
    // "auto", "hardware", "software"; applied when the app starts.
    Q_PROPERTY(QString decodePath READ decodePath WRITE setDecodePath NOTIFY changed)
    // "" means Omarchy's recordings folder.
    Q_PROPERTY(QString recordingsFolder READ recordingsFolder WRITE setRecordingsFolder NOTIFY changed)
    // Disk cache budget per store (thumbnails, waveforms) in MB: "256", "512", "1024", "4096".
    Q_PROPERTY(QString cacheLimit READ cacheLimit WRITE setCacheLimit NOTIFY changed)
    // Export encoder: "auto" (the GPU's where validated), "hardware", "software".
    Q_PROPERTY(QString exportEncoder READ exportEncoder WRITE setExportEncoder NOTIFY changed)
    // UI text size in percent: "100", "115", "130" (larger text, ui-ux-evaluation gates); the
    // QML multiplies its pixel sizes by textScale.
    Q_PROPERTY(QString textSize READ textSize WRITE setTextSize NOTIFY changed)
    Q_PROPERTY(double textScale READ textScale NOTIFY changed)
    // The decode path this run started with, to tell the user a change waits for a restart.
    Q_PROPERTY(QString startupDecodePath READ startupDecodePath CONSTANT)
    // Read-only facts for the screen: GPU, driver, active decode path and why, audio output,
    // environment overrides. Filled by main.cpp.
    Q_PROPERTY(QVariantMap system READ system NOTIFY systemChanged)
public:
    explicit AppSettings(QString settings_file, QObject* parent = nullptr);

    [[nodiscard]] QString previewQuality() const;
    [[nodiscard]] QString decodePath() const;
    [[nodiscard]] QString recordingsFolder() const;
    [[nodiscard]] QString startupDecodePath() const;
    [[nodiscard]] QString cacheLimit() const;
    [[nodiscard]] QString exportEncoder() const;
    [[nodiscard]] QString textSize() const;
    [[nodiscard]] double textScale() const;
    [[nodiscard]] QVariantMap system() const;

    void setPreviewQuality(const QString& value);
    void setDecodePath(const QString& value);
    void setRecordingsFolder(const QString& value);
    void setCacheLimit(const QString& value);
    void setExportEncoder(const QString& value);
    void setTextSize(const QString& value);
    void setSystem(QVariantMap system);
    Q_INVOKABLE void resetAll();
    // The screen asks for fresh system facts when it opens (the audio device may have changed).
    Q_INVOKABLE void refreshSystem();

signals:
    void changed();
    void systemChanged();
    void systemRequested();

private:
    void store(const char* key, const QString& value, const QString& fallback);

    QString settings_file_;
    QString preview_quality_;
    QString decode_path_;
    QString recordings_folder_;
    QString text_size_;
    QString cache_limit_;
    QString export_encoder_;
    QString startup_decode_path_;
    QVariantMap system_;
};
