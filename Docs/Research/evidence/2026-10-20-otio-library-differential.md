# OTIO import against the OpenTimelineIO library — 2026-10-20

**Classification:** INTEROP / correctness, differential. The oracle is the reference
implementation: OpenTimelineIO 0.18.1 (Arch `opentimelineio 0.18.1-3`) writes each timeline
and computes each clip's record position (`Track.range_of_child`) and source range.

**Method:** `tools/interop/otio_diff.py build/release/tools/oma-project/oma-project 500`.
Seeds 1-500 build random timelines: 1-4 tracks (video, and audio after the first), up to 8
items each, cuts with handles, gaps of 1-48 frames, centred SMPTE dissolves of 1-6 frames each
side, about one clip in ten `enabled: false`, at 23.976, 24, 25, 29.97 or 30 fps. The library
writes the `.otio`; `oma-project convert` makes an OmaMovie project against a generated minute
of picture and sound; every enabled clip's record start, length and source in must equal the
library's, exactly, as fractions of a second, per track kind; a disabled clip must be absent.

**Coverage (seeds 1-500, before audio/disabled were added):** 3627 clips, 889 gaps, 642
dissolves; each rate 95-105 seeds; 1, 2 and 3 tracks about equally.

**Result:** 500/500 seeds agree (Release build, working tree on `92999ea`), 18 s.

**Mutation check:** shifting every expected start by 1 ms makes 5/5 seeds fail. A first run
with silent media failed 251 seeds on audio tracks only (clips without sound cannot be placed on
an audio track): a fixture defect, fixed by giving the generated media sound.

**Mixed rates (`--mixed`):** later clips take random rates of the five (a 29.97 fps clip in a
24 fps sequence). Lengths off the sequence grid are rounded to the nearest tick and reported,
so the k-th clip of a track may sit up to k ticks (1/240000 s at NTSC rates) from the library's
exact position. 500/500 seeds within that bound; the 1 ms mutation fails 10/10. The first mixed
run failed 72 seeds because the oracle rounded the library's fractional frame values (3.125 s
is 78.125 frames at 25 fps) to whole frames; OmaMovie was right.

**Not covered:** time effects, nested stacks, `.otioz`/`.otiod` bundles (not read), and
exports from applications other than Kdenlive (see
[Kdenlive's exports](2026-10-20-kdenlive-otio-exports.txt)).
