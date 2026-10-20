# Final Cut Pro XML (FCPXML)

Final Cut Pro's interchange format: one XML document (`.fcpxml`) or a bundle folder (`.fcpxmld`)
whose document is `Info.fcpxml`. Read-only in OmaMovie (§16.6): `libs/importers`
(`import_fcpxml`) produces ProjectIR; nothing writes FCPXML. Written after Apple's published
reference for 1.10/1.11; **no document exported by Final Cut Pro itself has been checked** (no
Mac here), so the fixtures are hand-written to that reference.

## Structure read

| Element | Read as |
|---|---|
| `<fcpxml version>` | the version (report); the root must be `fcpxml` |
| `<format id frameDuration>` | a frame duration (`1001/30000s`); the sequence's becomes the IR timebase |
| `<asset id start duration src>` / `<media-rep kind="original-media" src>` (1.9+) | a media file and its available range |
| `library/event/project` or `event/project` | the first project's `sequence` is imported; others are reported |
| `<spine>`: `asset-clip`, `clip` (with a `video`/`audio` child), `gap` | the first IR track: clips and gaps in order |
| `<transition>` in the spine | a transition centred on the cut the spine has reached; a name with "Dissolve" becomes `SMPTE_Dissolve` |
| children with `lane` (connected clips) | one IR track per lane, placed at parent offset + (child offset - parent start); negative lanes are audio |

Times are rational seconds (`0s`, `3600s`, `1001/30000s`) read on the sequence's frame grid; a
value off the grid is rounded to the nearest frame and reported.

## Semantics and choices

- Spine offsets count from the first item's offset (a sequence's `tcStart` such as `3600s`
  becomes timeline zero).
- `filter-*` and `adjust-*` children of a clip are kept opaque on it and reported.
- Retiming (`timeMap`, `conform-rate`) is kept opaque and reported; the clip plays its
  `start`/`duration` at normal speed. Until 2026-10-20 it was dropped without a note. Constant
  speed is not mapped yet: without Final Cut here, which time the clip's `start` counts in
  under a `timeMap` cannot be checked against what Final Cut shows.
- Titles, generators and other spine or connected items are kept opaque on the timeline and
  reported; a spine item becomes a gap of its length, a connected one is left out.
- `media` resources (compound clips, multicam) are kept opaque and reported; clips that refer to
  them have no readable asset and become reported gaps.
- Overlapping connected clips on one lane: the later one is left out, reported.
- The app's file dialog picks `.fcpxml` files; a `.fcpxmld` bundle is a folder, which the dialog
  does not offer: `oma-project import/convert` read bundles, as does the app's import given one.

## Safety (§18)

The shared XML reader (`libs/importers/src/xml*`, [MLT notes](mlt.md)) takes no DTD. `fuzz_fcpxml`
runs in CI; its first run found a one-byte read past the end of a text ending after an
attribute's `=` (fixed in the reader for both formats; `tests/importers/test_xml_bounds.cpp`).

## Evidence

`tests/importers/test_fcpxml*.cpp` (`basic.fcpxml`: cuts, a cross dissolve, a gap, a connected
clip and title), `tests/app/test_import_fcpxml.cpp` (`relative.fcpxmld`: a bundle through the
app's import with its media found), `tests/project/test_cli.sh` (the same bundle through
`oma-project import` and `convert`).
