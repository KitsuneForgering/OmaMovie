// Headless tests of the app's Session (the editor state behind the QML), without a window:
// Qt's offscreen platform and private XDG folders, so the user's settings, recent projects,
// autosaves, caches and screen recordings are never touched.
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
void run_place_preview_tests();
void run_source_range_tests();
void run_edge_frames_tests();
void run_roll_cut_tests();
void run_slip_slide_tests();
void run_export_frames_tests();
void run_export_audio_tests();
void run_export_write_fail_tests();
void run_export_switch_tests();
void run_key_tracks_tests();
void run_layout_tests();
void run_marker_tests();
void run_effect_preset_app_tests();
void run_import_timeline_tests();
void run_scope_tests();
void run_scope_session_tests();
void run_parade_tests();
void run_color_interpretation_tests();
void run_color_session_tests();
void run_recording_workflow_tests();
void run_export_transfer_tests();

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // The desktop's theme and input-method plugins (GTK, dconf, AT-SPI, fcitx) are not part of
    // what the tests check, and CI has none: keep local runs the same.
    qunsetenv("QT_QPA_PLATFORMTHEME");
    qunsetenv("QT_IM_MODULE");
    QTemporaryDir home;
    // No session bus, as in CI: an export that finishes while the offscreen app is not active
    // would otherwise post a real desktop notification.
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=" + home.filePath("no-bus").toLocal8Bit());
    for (const char* name : {"XDG_CONFIG_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME", "XDG_DATA_HOME",
                             "OMARCHY_SCREENRECORD_DIR"}) {
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
    run_place_preview_tests();
    run_source_range_tests();
    run_edge_frames_tests();
    run_roll_cut_tests();
    run_slip_slide_tests();
    run_export_frames_tests();
    run_export_audio_tests();
    run_export_write_fail_tests();
    run_export_switch_tests();
    run_key_tracks_tests();
    run_layout_tests();
    run_marker_tests();
    run_effect_preset_app_tests();
    run_import_timeline_tests();
    run_scope_tests();
    run_scope_session_tests();
    run_parade_tests();
    run_color_interpretation_tests();
    run_color_session_tests();
    run_recording_workflow_tests();
    run_export_transfer_tests();
    return cest_result();
}
