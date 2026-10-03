# OmaMovie

OmaMovie is a video editor under development for Omarchy. Its current editor shell lets you import local videos and pictures, arrange clips on a timeline, preview the result, and adjust clip audio. It is useful for trying the editing workflow; it cannot yet save a project or export a movie.

## Try the editor

On an Omarchy or current Arch Linux system, from this repository:

```sh
make deps
make run-gui
```

`make deps` installs the packages declared in [PKGBUILD](PKGBUILD) using `pacman` and may request sudo. `make run-gui` builds and opens the Qt Quick editor. Use **Import** or `Ctrl+I` to choose a local file, then select it in the library and append it to the storyline. You can split at the playhead with `Ctrl+B`, undo with `Ctrl+Z`, and play or pause with Space.

You can also open a file directly:

```sh
make run-gui GUI_FILE=/path/to/video.mp4
```

The imported media stays on disk. The current session is temporary: closing the app loses timeline edits. The Export control is disabled until the render and project work in M7 is implemented.

## What works today

- Import and inspect local video and still image files through FFmpeg. Actual codec support depends on the installed FFmpeg build and the file.
- Append, insert, overwrite, split, trim, delete, and ripple edit clips with undo and redo.
- Preview the timeline with Vulkan compositing and audio clock playback; adjust clip gain, fades, and mute.
- Use visible transport and edit controls, or their keyboard shortcuts, in the editor shell.

The GUI currently uses software video decoding for its viewer. Hardware decode exists in the media library but is not yet connected to GUI playback. Project persistence, video export, external project interchange, and installable application packaging are planned work, not current capabilities.

## Build and development

Run `make test` for the library tests or `make help` for available targets. The [implementation plan](Docs/implementation-plan.md) tracks completed and pending milestones. [UI design](Docs/ui-design.md) describes the editor workflow. [Format boundaries](Docs/format-architecture.md) and [CLAUDE.md](CLAUDE.md) cover architecture and contributor rules.

License: [MIT](LICENSE).
