# Interoperability support levels

Support is declared per format **and version**, at the levels of CLAUDE.md §16.5, with the
tests that prove each level. A format with no proven level is not supported. The path is
external project → importer → ProjectIR (ADR-0008) → converter → OmaMovie project.

| Level | Meaning |
|---|---|
| 0 | File recognized and version identified |
| 1 | Referenced media and clips |
| 2 | Tracks, cuts, transitions |
| 3 | Transforms, audio (gain/fades), keyframes |
| 4 | Effects with a mapped equivalent |
| 5 | Round trip |

| Format | Versions | Level | Evidence |
|---|---|---|---|
| OpenTimelineIO JSON (`.otio`) | Timeline.1, Stack.1, Track.1, Clip.1/2, Gap.1, Transition.1, ExternalReference.1, MissingReference.1, RationalTime.1, TimeRange.1 | 2: imported in the app (Import OpenTimelineIO…) and reported by `oma-project import` | `tests/importers` (hand-written file per the OTIO specification), `fuzz_otio`, `tests/project` (`from_ir`), `tests/app/test_import_timeline.cpp`, GUI smoke; no application's actual export checked |
| FCPXML / `.fcpxmld` | — | none | Not started |
| CMX 3600 EDL | CMX 3600: cuts, dissolves, V/A/AA/B channels, FROM CLIP NAME, SOURCE FILE, drop-frame | 2 into ProjectIR only (`import_edl`); not yet in the app or `oma-project` | `tests/importers/test_edl*.cpp` (hand-written file, [format notes](formats/edl.md)), `fuzz_edl`; no application's actual export checked |
| Kdenlive (MLT XML) | — | none | Not started |

`libs/project-ir` (2026-10-13) holds the IR, its validation (`tests/project-ir`) and the import
report every importer fills. `libs/importers` reads OTIO into it: NTSC rates become exact
fractions, other float rates and sub-thousandth frame times are rounded and reported, effects,
markers, generators and nested stacks are kept as opaque objects and reported.

`project::from_ir` (2026-10-13) turns ProjectIR into a native project: the first video track
becomes the storyline, gaps stay as space, SMPTE dissolves become the next clip's transition
(other transitions become reported hard cuts), media the app cannot resolve leaves reported
gaps, and a timecode start is reported (sequences start at zero). Opaque objects are not saved
in the native project yet; the converter reports how many (2026-10-13) so the loss is never silent.
