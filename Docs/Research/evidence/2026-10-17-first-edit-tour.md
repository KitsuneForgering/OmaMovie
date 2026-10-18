# First edit tour and bundled demo — 2026-10-17

## UX / integration gap

The first-opening tour identified interface areas but did not teach the path from a source range to a saved and exported edit. It did not show how to open the Command Palette. A new user also had to supply media before seeing an editable sequence.

## Cause and change

The tour was a standalone explanation with no material to practice on. `WelcomeTour.qml` now presents six concrete steps with expected outcomes, uses the existing `OmaAction` names and shortcuts, opens the Command Palette directly, and hands off to a bundled three-shot project. F1 and visible Help/Tour controls reopen the guide. The demo is loaded through the normal project loader as an unsaved copy; Save therefore asks for the user's own destination. The package installs the project and its three small media files next to QML.

Changed: `apps/omamovie/qml/WelcomeTour.qml`, `Main.qml`, `ProjectsScreen.qml`, `Icon.qml`; `app_settings.*`, `main.cpp`, `session.*`, `CMakeLists.txt`; `apps/omamovie/demo/`; `scripts/generate-demo-project.sh`, `ci-run.sh`, `package-check.sh`; `tests/app/test_demo_project.cpp`, `main.cpp`, `apps/omamovie/src/gui_smoke.cpp`; onboarding and M6 documentation.

## Verification

- The app test opens the bundled project, marks and appends a source range, saves, reopens, exports with software encoding, then independently decodes the exported movie and checks the order of the three shots.
- GUI smoke checks first presentation, Command Palette and demo handoffs, manual reopening, completion persistence, keyboard flow, and a 640 px window with 130% text. The native smoke passed with the Intel Vulkan device on the second run. A first run lost window focus during its later keyboard segment; the onboarding and demo checks passed in both runs.
- The generation script first removes its four previous demo outputs, then regenerates and validates them. After this clean regeneration, `make test` passed 14/14 suites (113 seconds). `make format-check`, `git diff --check`, and shell syntax checks passed.
- A fresh `DESTDIR` install contained the tour, project, three media files, desktop entry, icon, and MIME file. Its first GUI smoke passed the tour and demo checks but failed an existing title pixel comparison; a repeat passed overall. Native GUI smoke passed its full keyboard path on the second run. The visual check and desktop focus can vary between runs, so the human walkthrough is still needed.
- PR CI is not yet green: the previous SHA failed the sustained memory threshold on Clang Debug despite stable file descriptors. This onboarding change is not claimed as a release gate pass.

## v0.1 impact and limit

The first edit can be explored without importing personal media, while shortcuts and edit consequences remain tied to the application action registry. Automated tests confirm behavior and export content; a person unfamiliar with OmaMovie still needs to perform the M6 walkthrough to establish whether the wording and sequence are understandable.
