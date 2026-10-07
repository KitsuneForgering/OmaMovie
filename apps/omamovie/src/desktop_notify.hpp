#pragma once

#include <QString>

// A desktop notification through the freedesktop Notifications D-Bus service (mako in
// Omarchy). Best effort: without a session bus or a notification daemon nothing happens.
// UI thread.
void notify_desktop(const QString& summary, const QString& body);
