#include "desktop_notify.hpp"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QStringList>
#include <QVariantMap>

void notify_desktop(const QString& summary, const QString& body) {
    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
    // app_name, replaces_id, app_icon, summary, body, actions, hints, expire_timeout (-1: default)
    call << QStringLiteral("OmaMovie") << 0U << QStringLiteral("omamovie") << summary << body << QStringList()
         << QVariantMap{{QStringLiteral("desktop-entry"), QStringLiteral("omamovie")}} << -1;
    // Asynchronous: a missing daemon must not block the UI thread.
    QDBusConnection::sessionBus().asyncCall(call);
}
