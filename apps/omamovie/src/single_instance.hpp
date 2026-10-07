#pragma once

#include <QLocalServer>
#include <QObject>
#include <QString>
#include <QStringList>

// One OmaMovie per user session (ui-design §3.1): a second launch hands its files to the
// running window and exits. The channel is a local socket in $XDG_RUNTIME_DIR (private to the
// user); a message is the absolute paths, one per line, UTF-8, ended by closing the
// connection. The receiver bounds what it reads, since any local process of the user can
// connect.
class SingleInstance : public QObject {
    Q_OBJECT
public:
    static constexpr qint64 kMaxMessageBytes = 64 * 1024;
    static constexpr qsizetype kMaxPaths = 256;

    explicit SingleInstance(QString socket_path, QObject* parent = nullptr)
        : QObject(parent), socket_path_(std::move(socket_path)) {}

    // $XDG_RUNTIME_DIR/omamovie.sock (or the temp directory without one).
    [[nodiscard]] static QString default_socket();

    // True when a running instance took `paths` (the caller then exits).
    [[nodiscard]] bool forward(const QStringList& paths) const;
    // Becomes the running instance; a stale socket left by a crash is replaced.
    bool listen();

    // Splits and validates a message: absolute paths only, at most kMaxPaths.
    [[nodiscard]] static QStringList parse(const QByteArray& message);

signals:
    // Files sent by another launch; empty when it was started without any (raise the window).
    void received(const QStringList& paths);

private:
    QString socket_path_;
    QLocalServer server_;
};
