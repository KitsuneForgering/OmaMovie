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
| `<producer mlt_service="timewarp">` with `warp_speed`, `warp_resource` | a constant speed change, as Kdenlive writes one: the clip plays `warp_resource` at `warp_speed` (read exactly as a decimal); reverse and unreadable speeds stay gaps, reported |
| `<filter>` inside a producer, `<link>` inside a chain | kept opaque on each of its clips; reported. A `timeremap` link (Kdenlive's time remap) gets its own note: the clip plays at normal speed for its length. Until 2026-10-20 links were dropped without a note |
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
- `hide="video"` on a track or `kdenlive:audio_track` on a playlist makes an audio track;
  `hide="both"` imports the track hidden and muted. Kdenlive's other hide combinations for
  hidden video or muted audio tracks are not distinguished yet (no Kdenlive here to author them).
- Colour and black producers are generators: their entries stay as gaps, reported.
- Transitions and filters on the main tractor (compositing, audio mixing between tracks) are kept
  opaque on the timeline and reported.
- Clips of one resource share one media reference.
- A filter with `disable=1`, an `audiolevel` meter and Kdenlive's automatic track compositing
  (`internal_added=237`: tracks stacked, their sound summed, as OmaMovie does) change nothing
  OmaMovie would not do: they are kept (counted among the objects the project cannot save) but
  not reported one by one. Kdenlive adds a disabled `volume` and `panner`, a meter and one
  compositing transition to every track. A transition is a same-track mix only inside a tractor
  of playlists (Kdenlive's A/B pair); in the sequence tractor Kdenlive 23.04+ nests in the main
  one it is between tracks
  ([2026-10-20 rerun](../Research/evidence/2026-10-20-kdenlive-dataset-import.txt)).
- A timewarp's frames (its in point, length and entry points) are source frames divided by the
  speed: `timewarp:2` frames 1-5 are source frames 2-10. The imported clip keeps the entry's
  length at the profile rate and its source range in source time. At half speed melt shows the
  source frame nearest each half position, rounding halves up (4 5 5 6 6 7 for source 4-6.5);
  OmaMovie keeps the frame already on screen (4 4 5 5 6 6). Sound follows the speed with its
  pitch, as in melt's default; a speed the mixer cannot play with sound (0.7×: no whole audio
  rate) gets an import note.

## Safety (§18)

The XML reader (`libs/importers/src/xml*`) takes no DTD at all, so no external entity and no
entity expansion; only the five predefined and numeric character references; depth 64, one
million elements, 64 MiB. `fuzz_mlt` runs in CI.

## Evidence

`tests/importers/fixtures/melt_mix.mlt` is melt 7.40's own output (root path replaced);
`melt_timewarp_23976.mlt` is melt 7.42's speed change at 2×, ½× and 1×, exported by OmaMovie
after save and reopen and compared with melt's render frame by frame
(`tests/app/test_shared_speed.cpp`, [evidence](../Research/evidence/2026-10-20-mlt-timewarp.md));
`tools/bench/cross-editor/roundtrip.sh` makes a document with melt, renders it with melt and
with OmaMovie after `oma-project convert`, and compares the movies frame by frame. Kdenlive's own
test projects (MLT 7.7-7.23: av, test-mix, clip-ids, test-nesting) read to ProjectIR with every
skipped object named ([evidence](../Research/evidence/2026-10-15-kdenlive-dataset-import.txt)):
Kdenlive keeps same-track mixes inside per-track tractors and track audio filters there too, both
reported; its black background track is a colour generator. Not imported yet: same-track mixes
as dissolves, nested sequences.

**Kdenlive's same-track mix, as found in `test-mix.kdenlive` (MLT 7.9):** each timeline track is
a tractor of two playlists (A, B). Clips alternate between them and overlap on the record where
they mix; each mix is a `luma` transition on that tractor with its own in/out (clock values) and
`kdenlive:mixcut` (where the cut sits inside the mix), sometimes `reverse=1`. A clip on B may span
several clips on A. Turning that into one IR track needs the visible clip at every instant, then
a dissolve per transition; without Kdenlive here to author reference projects that cannot be
checked against what Kdenlive shows, so the importer keeps both playlists as separate IR tracks
and reports each mix rather than guess.
