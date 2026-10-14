#pragma once

// The --smoke run (ui-design §8, M10): drives the live editor window through import, edits,
// playback, keyboard paths and recovery, printing "GUI smoke: PASS" or what failed. It lives
// apart from startup so production code does not carry the test driver; main() still owns
// every object and launches it unchanged.

#include <QString>

class ActionRegistry;
class AppSettings;
class PreviewItem;
class QGuiApplication;
class QQuickWindow;
class QTemporaryDir;
class Session;
namespace oma::gpu {
class Device;
}

// What main() set up for the window; all of it outlives the run.
struct SmokeContext {
    QGuiApplication& application;
    QQuickWindow* window;
    PreviewItem* preview;
    Session& session;
    oma::gpu::Device& device;
    ActionRegistry& actions;
    AppSettings& settings;
    QTemporaryDir& recordings; // the run's private recordings, state and settings folder
};

// A fake Omarchy for the smoke run: theme state and scripts under `root`.
void write_smoke_theme(const QString& root);

// Runs the event loop until the smoke finishes; returns the process exit code.
[[nodiscard]] int run_gui_smoke(const SmokeContext& context);
