// Headless tests of the app's Session (the editor state behind the QML), without a window:
// Qt's offscreen platform and private XDG folders, so the user's settings, recent projects,
// autosaves and caches are never touched.
#include <QDir>
#include <QGuiApplication>
#include <QTemporaryDir>

#include "oma_test.hpp"

void run_session_tests();

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QTemporaryDir home;
    for (const char* name :
         {"XDG_CONFIG_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME", "XDG_DATA_HOME"}) {
        QDir(home.path()).mkpath(QString::fromLatin1(name));
        qputenv(name, home.filePath(QString::fromLatin1(name)).toLocal8Bit());
    }
    int qt_argc = 1;
    QGuiApplication app(qt_argc, argv);
    cest_init(argc, argv);
    run_session_tests();
    return cest_result();
}
