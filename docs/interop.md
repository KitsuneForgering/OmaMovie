# Interoperability support levels

Support is declared per format **and version**, at the levels below, with the
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
| FCPXML / `.fcpxmld` | 1.10/1.11 per Apple's reference (formats, assets with `media-rep`, first project's spine: asset-clip, clip, gap, transition; connected clips by lane) | 2: in the app (Import Timeline…, `.fcpxml`) and `oma-project import/convert` (libs/ingest, `.fcpxmld` bundles too) | `tests/importers/test_fcpxml*.cpp` (hand-written files, [format notes](formats/fcpxml.md)), `tests/app/test_import_fcpxml.cpp`, `fuzz_fcpxml`; no document exported by Final Cut Pro checked |
| CMX 3600 EDL | CMX 3600: cuts, dissolves, V/A/AA/B channels, FROM CLIP NAME, SOURCE FILE, drop-frame | 2: in the app (Import Timeline…, which asks for the frame rate) and `oma-project import/convert FILE.edl [RATE]` (libs/ingest, media probed) | `tests/importers/test_edl*.cpp` (hand-written file, [format notes](formats/edl.md)), `tests/project/test_cli.sh`, `tests/app/test_import_edl.cpp`, `fuzz_edl`; no application's actual export checked |
| MLT XML / Kdenlive (`.mlt`, `.kdenlive`) | MLT 7.40 as melt writes it: profile, producers/chains, playlists (entries, blanks), tractors (tracks, nested track tractors), melt mixes | 2: in the app (Import Timeline…) and `oma-project import/convert` (libs/ingest) | `tests/importers/test_mlt.cpp` (melt's own output), `tests/app/test_import_timeline.cpp` (`relative.mlt`, [format notes](formats/mlt.md)), `tools/bench/cross-editor/roundtrip.sh` (melt and OmaMovie render the same document: cuts match frame for frame, [evidence](Research/evidence/2026-10-15-mlt-roundtrip.txt)), `fuzz_mlt`; Kdenlive's own test projects (7.7-7.23) read with every skipped object reported ([evidence](Research/evidence/2026-10-15-kdenlive-dataset-import.txt)) |

`libs/project-ir` (2026-10-13) holds the IR, its validation (`tests/project-ir`) and the import
report every importer fills. `libs/importers` reads OTIO into it: NTSC rates become exact
fractions, other float rates and sub-thousandth frame times are rounded and reported, effects,
markers, generators and nested stacks are kept as opaque objects and reported.

`project::from_ir` (2026-10-13) turns ProjectIR into a native project: the first video track
becomes the storyline, gaps stay as space, SMPTE dissolves become the next clip's transition
(other transitions become reported hard cuts), media the app cannot resolve leaves reported
gaps, and a timecode start is reported (sequences start at zero). Opaque objects are not saved
in the native project yet; the converter reports how many (2026-10-13) so the loss is never silent.
