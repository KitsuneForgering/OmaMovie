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

[Continue in omarchy-adapters.md](omarchy-adapters.md)
