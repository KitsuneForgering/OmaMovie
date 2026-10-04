# OmaMovie

OmaMovie is an in-progress video editor for people using [Omarchy](https://omarchy.org/). The goal is to import everyday video and screen recordings, cut them on a timeline, and export a finished video through a native desktop interface.

**Current status:** there is no editor window or `omamovie` executable yet. This repository currently contains C++ libraries for time and jobs, media decoding, Vulkan resources, and compositing, plus tests and hardware experiments. You cannot edit or export a video with it today. The first usable editor is planned for v0.1; see the [implementation plan](Docs/implementation-plan.md).

## What is here now

- `libs/base` handles rational time, errors, logging, and cancellable jobs.
- `libs/media` probes and decodes video and audio with FFmpeg.
- `libs/gpu` and `libs/compositor` provide Vulkan resources and CPU/GPU compositing code.
- Tests cover these components; the hardware experiments and their results are in [`Docs/spikes/`](Docs/spikes/).

The intended first release adds the timeline, playback, project saving, export, and the desktop interface. Those are development goals, not current features.

## Build and run the current tests

On an up-to-date Arch Linux or Omarchy system, install the build and test dependencies listed in [PKGBUILD](PKGBUILD): a C++23 compiler, GNU Make, FFmpeg, Vulkan headers and loader, shaderc, and a Vulkan driver (or `vulkan-swrast` for software Vulkan tests). The repository's dependency helper uses `sudo pacman`; review its package list before running it.

```sh
git clone https://github.com/KitsuneForgering/OmaMovie.git
cd OmaMovie
make test
```

`make test` builds the libraries and test binaries, generates media fixtures with FFmpeg, and runs the test suites. It does not launch a video editor. Run `make help` for the available build targets. Hardware dependent tests may need a working Vulkan device or software Vulkan driver.

## Project details

- [Implementation plan](Docs/implementation-plan.md) — milestones, current work, and release criteria.
- [Architecture decisions](Docs/adr/README.md) — build, time representation, threading, and GPU policies.
- [Research](Docs/Research/README.md) — product and hardware research.
- [Development guide](CLAUDE.md) — engineering rules and module boundaries; its repository status section may lag behind the code.

The project targets Omarchy on Arch Linux. Windows, macOS, and X11 support are outside its current scope. It is licensed under the [MIT license](LICENSE).
