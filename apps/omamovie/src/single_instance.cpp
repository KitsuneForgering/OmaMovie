#include "single_instance.hpp"

#include <QDir>
#include <QFileInfo>
#include <QLocalSocket>
#include <QStandardPaths>

#include <memory>

namespace {
constexpr int kConnectMs = 300;
constexpr int kWriteMs = 1000;
} // namespace

QString SingleInstance::default_socket() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty()) dir = QDir::tempPath();
    return dir + QStringLiteral("/omamovie.sock");
}

bool SingleInstance::forward(const QStringList& paths) const {
    QLocalSocket socket;
    socket.connectToServer(socket_path_);
    if (!socket.waitForConnected(kConnectMs)) return false;
    QByteArray message;
    for (const QString& path : paths) message += QFileInfo(path).absoluteFilePath().toUtf8() + '\n';
    socket.write(message);
    if (!socket.waitForBytesWritten(kWriteMs)) return false;
    socket.disconnectFromServer();
    if (socket.state() != QLocalSocket::UnconnectedState) socket.waitForDisconnected(kWriteMs);
    return true;
}

bool SingleInstance::listen() {
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server_.listen(socket_path_)) {
        // forward() already failed to connect, so nobody serves it: a leftover from a crash.
        QLocalServer::removeServer(socket_path_);
        if (!server_.listen(socket_path_)) return false;
    }
    connect(&server_, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket* socket = server_.nextPendingConnection()) {
            auto buffer = std::make_shared<QByteArray>();
            connect(socket, &QLocalSocket::readyRead, socket, [socket, buffer] {
                *buffer += socket->read(kMaxMessageBytes - buffer->size());
                if (buffer->size() >= kMaxMessageBytes) socket->abort(); // oversized: dropped
            });
            connect(socket, &QLocalSocket::disconnected, this, [this, socket, buffer] {
                if (buffer->size() < kMaxMessageBytes) emit received(parse(*buffer));
                socket->deleteLater();
            });
        }
    });
    return true;
}

QStringList SingleInstance::parse(const QByteArray& message) {
    QStringList paths;
    for (const QByteArray& line : message.split('\n')) {
        if (line.isEmpty()) continue;
        const QString path = QString::fromUtf8(line);
        if (!QDir::isAbsolutePath(path)) continue;
        paths.append(path);
        if (paths.size() == kMaxPaths) break;
    }
    return paths;
}
