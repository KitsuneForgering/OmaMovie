# Omarchy integration: installed evidence and adapter boundaries

> Reviewed on 2026-10-03 with skeptical-research. This is non-normative research.
> Source metadata and access limits: [source register](sources.md). Decisions and open
> validation gates: [audit](skeptical-review.md). Product descriptions are not user studies.

## 1. Conclusion

The installed Omarchy scripts provide useful theme, recording and opacity conventions.
They are version-sensitive integration evidence, not stable cross-version APIs. Keep them
in the app adapter with timeouts/fallbacks. No user/system configuration was modified.

## 2. Provenance

On 2026-10-03 `pacman -Q omarchy` reports **4.0.4-1**, whereas
`/usr/share/omarchy/version` contains **4.0.0.alpha**. The mismatch is recorded, not resolved
by assuming a clean upstream release. [Environment and source hashes](evidence/2026-10-03-local.txt)
identify the installed files inspected; no upstream commit identity was available.

The base package list contains Kdenlive. That supports an installer-default observation,
not a statement about all existing installations or demand for its replacement. The GPU
installer scripts select packages by detected hardware; installed Vulkan packages alone
do not guarantee every codec/profile works.

## 3. Theme and font

The local `omarchy-theme-color` documents `--all` resolved key/tab/value output and reads
`~/.local/state/omarchy/current/theme/colors.toml`. Invoke occasionally off the hot path,
parse defensively, and keep semantic UI tokens/fallback palette in one place.

The local `omarchy-theme-set` removes the current directory and then moves the staged one,
then writes `theme.name`. **This is not an atomic directory replacement**: there is a missing-
directory interval. Watch the parent, debounce, retry/re-register watches and retain the
last good palette during incomplete updates. Theme files must not tint preview imagery.

The Omarchy font is an existing product choice. A shell `base-size` is not established as
an application font contract; use an app default until that mapping is tested. The adapter
must also work when scripts/files are absent in headless tests.

## 4. Opacity and display correctness

Local `default/hypr/windows.lua` tags ordinary windows for opacity `0.985 0.96`; the Resolve
rule removes that tag and sets `1 1`. These support an analogous documented app-specific
rule for the installed Lua configuration, not all Hyprland versions. A stable `omamovie`
Wayland app_id should be checked in the actual Qt window before shipping the rule.

Full opacity removes wallpaper blending; it does not ensure color calibration, display
transfer, gamut or HDR correctness. Document the rule without installing it automatically.
Native Qt/Wayland and any compositor-specific syntax need versioned integration tests.

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
