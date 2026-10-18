## 5. Recordings

Local `omarchy-capture-screenrecording` selects
`${OMARCHY_SCREENRECORD_DIR:-${XDG_VIDEOS_DIR:-$HOME/Videos}}`, passes auto codec selection,
60 fps CFR intent and CPU-encoding fallback. AAC is added only when audio devices are selected.
Therefore recordings may have **no audio**, and encoder intent is not a guarantee about the
file actually produced. Probe timestamps/codecs/color, including dropped frames/VFR inputs.

The Recordings source must respect the directory override rather than assuming `~/Videos`.
Do not index a still-growing capture as a completed file without retry/stability handling.
Test actual locally generated captures and no-audio files; do not commit private recordings.

## 6. Notifications, shell and packaging

Standard D-Bus notifications and `.desktop`/MIME integration are preferred app boundaries.
Shell progress plugins and dynamic menu providers are later optional integrations. Their
old manifest/IPC examples were not revalidated as stable public contracts and are no longer
copypaste guidance. An OmaStore hook on one machine does not establish release installability;
validate the manifest/artifacts separately when publishing is actually in scope.

The PKGBUILD remains the dependency source of truth. Read-only docs auditing does not
install packages, hooks, plugins, opacity rules or propose upstream changes automatically.

## 7. Proposed verification

In M6 test missing scripts, malformed/partial palettes, rapid theme changes, different fonts,
wide/half/narrow windows, custom recording directories, no-audio capture, and actual app_id.
Compare a standard Qt palette/file chooser baseline. Accept integration only if editing
remains usable when the optional platform input fails. These behavioral tests did not run
here; the evidence is installed-source inspection, not execution of the installer.

[Return to omarchy-integration.md](omarchy-integration.md)
