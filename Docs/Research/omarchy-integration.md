# Omarchy integration

> Research done on 2026-10-02 by **inspecting the local Omarchy 4.0.4 installation**
> (`/usr/share/omarchy`, `~/.config/omarchy`, `~/.local/state/omarchy`). Paths and formats may
> change between Omarchy versions. Every integration must have a fallback and live in a single
> app module (`CLAUDE.md` §11).

---

## 1. Summary

Omarchy 4 offers concrete integration points that are stable enough for OmaMovie to use:

| Point | Mechanism | Use in OmaMovie |
|---|---|---|
| **Theme** | `~/.local/state/omarchy/current/theme/colors.toml` + `omarchy-theme-color` | UI colors, live switching |
| **Window opacity** | Hyprland rules in Lua | **A 100% opaque window** (correct preview color) |
| **Screen recordings** | `gpu-screen-recorder` → `$XDG_VIDEOS_DIR` | A "Recordings" source in the library |
| **Shell** (Quickshell/QML) | Plugins with `manifest.json`, IPC | Export progress widget (optional) |
| **Omarchy menu** | `~/.config/omarchy/extensions/omarchy-menu.jsonc` | Shortcuts: new project, edit last recording |
| **Notifications** | `omarchy-notification-send` / D-Bus | "Export finished" |
| **Hooks** | `~/.config/omarchy/hooks/<name>.d/` | React to theme and font changes |
| **Drivers** | Omarchy's per-vendor installer | Know which hardware path to expect |

Product finding: **Kdenlive is installed by default on Omarchy** (`install/omarchy-base.packages`),
together with OBS, mpv, `gpu-screen-recorder` and `ffmpegthumbnailer`. OmaMovie competes for
that default editor slot.

---

## 2. Omarchy 4 technical context

- Arch Linux + Hyprland; **the Hyprland configuration is written in Lua** (`default/hypr/*.lua`).
- **`omarchy-shell`**: a single **Quickshell** (QML) instance that hosts the bar, menus, panels,
  notifications and overlays as plugins. **It is the same UI technology as OmaMovie (Qt Quick/QML).**
- More than 440 `omarchy-*` commands in `/usr/share/omarchy/bin`, with metadata (`omarchy:summary`, `omarchy:args`).
- Installed as a package (`omarchy 4.0.4-1`), with its own kernel (`linux-omarchy`).

---

## 3. Theme

### Format
Each theme's `colors.toml` defines:
```toml
mode = "dark"
accent = "#89b4fa"
selection = "#45475a"
muted = "#585b70"
background = "#1e1e2e"          # + dark_background, darker_background, lighter_background
foreground = "#cdd6f4"          # + dark_foreground, light_foreground, bright_foreground
red/yellow/orange/green/cyan/blue/magenta/brown (+ bright_* variants)
```

### How a theme is applied (`omarchy-theme-set`)
1. Assembles the theme in `~/.local/state/omarchy/current/next-theme` (official theme + user overlay).
2. Generates configs from `*.tpl` templates (`{{ accent }}`, `{{ accent_rgb }}`, `{{ mix a b 30% }}`...).
3. **Atomic swap**: `mv next-theme → theme`, writes `theme.name`.
4. Sends the palette to the shell over IPC, restarts/retints known apps (`omarchy-theme-set-obsidian`, `-vscode`...).
5. Runs `omarchy-hook theme-set <name>` (scripts in `~/.config/omarchy/hooks/theme-set.d/`).

### Color resolver
`omarchy-theme-color --all` prints `key<TAB>value` for **every resolved color** (aliases,
legacy names, derived shades, light/dark mode detection). It is the same resolver the templates use.

### Recommendation for OmaMovie
- **Read the palette with `omarchy-theme-color --all`**: no TOML parser dependency and the same
  resolution as Omarchy. Fallback: read `colors.toml` directly; final fallback: a built-in
  palette (the app must work outside Omarchy and in tests).
- **Live switching**: watch `~/.local/state/omarchy/current/` (the `theme` directory is
  **replaced** with `mv`, so watch the parent; `theme.name` changes on every switch) with
  `QFileSystemWatcher`, debounced. This avoids installing hooks in the user's directory.
- Map the palette to semantic UI tokens (panel background, selection, accent, secondary text) in
  a single place (a `ThemeProvider` exposed to QML).
- **Never theme color-critical areas**: preview, scopes, color picker and thumbnails show the
  real image. The theme only affects the surrounding interface.
- Fonts: `omarchy-font-current` and the `font-set.d` hook.

---

## 4. Window opacity (critical for color)

Omarchy applies translucency to **every** window:
```lua
o.window(".*", { tag = "+default-opacity" })
o.window({ tag = "default-opacity" }, { opacity = "0.985 0.96" })
```
Omarchy itself makes an exception for DaVinci Resolve (`default/hypr/apps/davinci-resolve.lua`):
> "Kept fully opaque: the default translucency distorts colour-critical grading work."

**OmaMovie needs the same exception.** A 96% opaque window blends the wallpaper into the preview
and invalidates any color judgment.

- The app sets a **stable Wayland app_id**: `omamovie` (through `QGuiApplication::setDesktopFileName("omamovie")`).
- Required rule:
  ```lua
  o.window("omamovie", { tag = "-default-opacity", opacity = "1 1" })
  ```
- Where to put it: short term, document it for users (Hyprland config in `~/.config/hypr`);
  medium term, **propose upstream** a `default/hypr/apps/omamovie.lua`, as exists for Resolve.
- A Wayland client does not control the opacity applied by the compositor.

---

## 5. Screen recordings

`omarchy-capture-screenrecording` (default shortcut **ALT+PRINT**) uses `gpu-screen-recorder`:
```
gpu-screen-recorder ... -k auto -f 60 -fm cfr -fallback-cpu-encoding yes -o <file> -a <audio> -ac aac
```
- Output in `${OMARCHY_SCREENRECORD_DIR:-$XDG_VIDEOS_DIR}` (usually `~/Videos`), named
  `screenrecording-YYYY-MM-DD_HH-MM-SS.mp4`.
- Codec chosen by GPU (`-k auto`: H.264/HEVC/AV1), **60 fps CFR**, **AAC** audio (desktop and/or microphone).
- Optional webcam overlay (picture-in-picture) already burned into the video.

### Recommendations
- **A "Recordings" source in the MediaPanel**: watches the video folder and shows recent
  recordings. It is the most natural flow for creators on Omarchy (record → edit).
- Make sure the decode path accepts exactly these files (H.264/HEVC/AV1 + AAC). Use real
  recordings as test fixtures (generated locally, not committed if large).
- **Future / upstream**: an "Edit in OmaMovie" notification when recording stops
  (`omarchy-notification-send` supports `--exec`). Today the script has no post-recording hook;
  that would need a contribution to Omarchy.
- Recording the webcam and the screen **separately** (two tracks) would give more control when
  editing than a pre-composited webcam. An idea to propose to Omarchy, or for recording built
  into OmaMovie itself (through PipeWire/the ScreenCast portal).

---

## 6. Shell (Quickshell): plugins and IPC

Plugins live in `~/.config/omarchy/plugins/<id>/` with a `manifest.json`:
```json
{
  "schemaVersion": 1,
  "id": "io.github.<author>.<name>",
  "kinds": ["bar-widget"],
  "entryPoints": { "barWidget": "BarWidget.qml" }
}
```
Kinds: `bar-widget`, `panel`, `overlay`, `menu`, `service`, `bar`. Installed from a git
repository or by hand + `omarchy-shell shell rescanPlugins`. Third-party plugins receive
capability-scoped facades.

IPC: `omarchy-shell shell summon|hide|toggle|call <id> ...`, `listPlugins`, `reloadConfig`.

### Recommendations
- **Optional and later**: a `bar-widget` plugin showing export/background render progress, so
  the user can close or minimize the editor.
- App → widget communication through a simple channel (a state file in `$XDG_RUNTIME_DIR` or
  D-Bus). The plugin must not contain editor logic.
- Not a priority. The app must be complete without the shell.

---

## 7. Menu, notifications and shortcuts

- **Menu**: `~/.config/omarchy/extensions/omarchy-menu.jsonc` accepts entries with `icon`,
  `label`, `action`, `when`, `provider` (dynamic rows from a command that returns JSON). Examples
  for OmaMovie: "OmaMovie › New project", "› Recent projects" (provider), "› Edit last recording".
- **Notifications**: use the freedesktop standard (`org.freedesktop.Notifications` over D-Bus),
  which the Omarchy shell serves. `omarchy-notification-send` is a convenient wrapper, but
  coupling to the script is unnecessary.
- **Launching**: `omarchy-launch-or-focus` (focuses the window if already open) is Omarchy's
  pattern for app shortcuts.

---

## 8. Drivers installed by Omarchy (what to expect on each machine)

| Vendor | Omarchy installs | Implication |
|---|---|---|
| **Intel** | `vulkan-intel`, `intel-media-driver` (VA-API iHD), `libvpl`, `vpl-gpu-rt` (QSV) | VA-API, Vulkan Video and QSV available |
| **AMD** | `vulkan-radeon` (VA-API ships with Mesa) | VA-API and Vulkan Video (RADV) available |
| **NVIDIA** (with GSP) | `nvidia-open-dkms`, `nvidia-utils`, `libva-nvidia-driver`; envs `LIBVA_DRIVER_NAME=nvidia`, `NVD_BACKEND=direct` | NVDEC/NVENC and Vulkan; VA-API through a translation layer |
| **NVIDIA** (without GSP) | Legacy 580xx driver | More limited support |
| All | **No OpenCL runtime**, no CUDA toolkit | Do not depend on the CUDA toolkit by default (OpenCL was removed from the project) |

---

## 9. Packaging and distribution

- **Arch/PKGBUILD** as the primary format (Omarchy is Arch).
- **OmaStore**: there is an `omastore.hook` in `post-update.d` on this machine. An `omastore.toml`
  and compatible releases would let OmaMovie be installed from the store.
- **`.desktop` + MIME**: `omamovie.desktop` (same app_id), a MIME type for the project format and
  "Open with" association for videos.
- **Portals**: `xdg-desktop-portal-hyprland` and `-gtk` are installed; the file chooser and
  ScreenCast through the portal work without Hyprland-specific code.
- **Long-term goal**: propose OmaMovie to Omarchy as the default editor (instead of Kdenlive),
  including the opacity rule and an `omarchy-theme-set-omamovie`, if it makes sense.

---

## 10. Rules for the code

- Every Omarchy integration lives in a single app module (e.g. `apps/omamovie/src/platform/omarchy/`), **never** in the libs.
- Every Omarchy path read has a fallback; the app works on Hyprland without Omarchy and in headless tests.
- Do not write to `~/.config/omarchy` without an explicit user action (e.g. an "Add to the Omarchy menu" button).
- Do not run `omarchy-*` scripts on the hot path; only occasional reads (theme, font).

---

## Sources (local files, Omarchy 4.0.4)

- `/usr/share/omarchy/bin/omarchy-theme-set`, `omarchy-theme-set-templates`, `omarchy-theme-color`, `omarchy-hook`, `omarchy-notification-send`, `omarchy-capture-screenrecording`
- `/usr/share/omarchy/themes/*/colors.toml`, `/usr/share/omarchy/default/themed/*.tpl`
- `/usr/share/omarchy/default/hypr/windows.lua`, `apps/davinci-resolve.lua`, `nvidia.lua`, `bindings/utilities.lua`
- `/usr/share/omarchy/shell/README.md`
- `/usr/share/omarchy/install/omarchy-base.packages`, `install/hardware/{vulkan.sh,nvidia.sh,intel/video-acceleration.sh}`
- `~/.config/omarchy/extensions/omarchy-menu.jsonc`, `~/.config/omarchy/plugins/*/manifest.json`, `~/.config/omarchy/hooks/`
- [Quickshell](https://quickshell.org/)
