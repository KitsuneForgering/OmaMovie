// Headless tests of the app's Session (the editor state behind the QML), without a window:
// Qt's offscreen platform and private XDG folders, so the user's settings, recent projects,
// autosaves and caches are never touched.
#include <QDir>
#include <QGuiApplication>
#include <QTemporaryDir>

#include "oma_test.hpp"

void run_session_tests();
void run_project_unknown_tests();
void run_project_recovery_tests();
void run_missing_lut_tests();
void run_export_bundle_tests();
void run_export_step_tests();
void run_source_playback_tests();
void run_edit_workflow_tests();
void run_export_success_tests();
void run_export_crash_tests();
void run_export_snapshot_tests();
void run_trim_preview_tests();
void run_recordings_tests();

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
    run_project_unknown_tests();
    run_project_recovery_tests();
    run_missing_lut_tests();
    run_export_bundle_tests();
    run_export_step_tests();
    run_source_playback_tests();
    run_edit_workflow_tests();
    run_export_success_tests();
    run_export_crash_tests();
    run_export_snapshot_tests();
    run_trim_preview_tests();
    run_recordings_tests();
    return cest_result();
}
