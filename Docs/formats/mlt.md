# MLT XML and Kdenlive projects

MLT is the engine behind Kdenlive and Shotcut; `melt` reads and writes its XML, and a
`.kdenlive` file is MLT XML with `kdenlive:` properties. Read-only in OmaMovie (§16.6):
`libs/importers` (`import_mlt`) produces ProjectIR; nothing writes MLT.

## Structure read

| Element | Read as |
|---|---|
| `<mlt version title root>` | the version (report), the timeline name, the folder relative resources join |
| `<profile frame_rate_num frame_rate_den>` | the timeline's rate (25 fps when absent, reported) |
| `<producer id in>` / `<chain id in>` with `resource`, `mlt_service`, `length`, `kdenlive:clipname` | a media file, its in point, its length (the IR's available range) and name |
| `<filter>` inside a producer | kept opaque on each of its clips; reported |
| `<playlist id>` with `<entry producer in out>` and `<blank length>` | an IR track: clips and gaps |
| `<tractor id in out>` with `<track producer hide>` and `<transition>` | the last tractor is the main one; its tracks are playlists or, as Kdenlive writes them, tractors of playlists |

Times are frames (`5`), clock values (`00:00:01.500`, rounded to the nearest frame at the
profile's rate) or timecode (`00:00:01:15`). **An entry's in/out count from its producer's in
point**: melt wrote `<producer in="5">` with `<entry in="0" out="9">` and rendered source frames
5-14 (checked 2026-10-15 by rendering the document and comparing frames); Kdenlive's producers
start at 0, where both readings agree.

## Semantics and choices

- melt's mix (`melt A B -mix N`) is a nested tractor of the outgoing and incoming producers'
  overlapping frames plus a `luma`/`mix` transition. The previous clip takes its own frames on
  and an `SMPTE_Dissolve` of the overlap's length ends at the next entry; the converter centres
  it on the cut (reported), so the dissolve's frames differ from melt's while the cut points and
  every frame outside it match.
- `hide="video"` on a track or `kdenlive:audio_track` on a playlist makes an audio track.
- Colour and black producers are generators: their entries stay as gaps, reported.
- Transitions and filters on the main tractor (compositing, audio mixing between tracks) are kept
  opaque on the timeline and reported.
- Clips of one resource share one media reference.

## Safety (§18)

The XML reader (`libs/importers/src/xml*`) takes no DTD at all, so no external entity and no
entity expansion; only the five predefined and numeric character references; depth 64, one
million elements, 64 MiB. `fuzz_mlt` runs in CI.

## Evidence

`tests/importers/fixtures/melt_mix.mlt` is melt 7.40's own output (root path replaced);
`tools/bench/cross-editor/roundtrip.sh` makes a document with melt, renders it with melt and
with OmaMovie after `oma-project convert`, and compares the movies frame by frame. Kdenlive's own
test projects (MLT 7.7-7.23: av, test-mix, clip-ids, test-nesting) read to ProjectIR with every
skipped object named ([evidence](../Research/evidence/2026-10-15-kdenlive-dataset-import.txt)):
Kdenlive keeps same-track mixes inside per-track tractors and track audio filters there too, both
reported; its black background track is a colour generator. Not imported yet: same-track mixes
as dissolves, nested sequences.
