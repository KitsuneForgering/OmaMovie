# CMX 3600 EDL

An edit decision list: plain text, one event per line, exported by Resolve, Premiere, Avid and
most editors for conform and hand-off. Read-only in OmaMovie (§16.6): `libs/importers`
(`import_edl`) produces ProjectIR; nothing writes EDL.

## Structure read

| Line | Form | Read as |
|---|---|---|
| Title | `TITLE: name` | the IR timeline's name |
| Frame code mode | `FCM: DROP FRAME` / `FCM: NON-DROP FRAME` | drop-frame for the timecodes that follow |
| Event | `001  REEL  V  C  00:00:01:00 00:00:03:00 01:00:00:00 01:00:02:00` | number, reel, channels, kind, source in/out, record in/out |
| Transition event | `002  REEL  V  D  012  src_in src_out rec_in rec_out` | a dissolve of 12 frames into this event |
| Clip name | `* FROM CLIP NAME: camera.mov` | the clip's name and, without a source file, its media |
| Source file | `* SOURCE FILE: /media/camera.mov` | the media target (Resolve writes it) |
| Motion | `M2   REEL  050.0  01:00:05:00` | kept opaque on the event's clip; reported (speed not imported) |

Channels: `V` (picture), `A`/`A1`, `A2`…`A9`, `AA` (A1 and A2), `B` or `A/V` (picture and A1),
`AA/V`. Timecodes are `HH:MM:SS:FF`; `;` or `.` before the frames marks drop-frame (SMPTE 12M:
frames 0 and 1 of each minute not divisible by ten are skipped; four at 59.94).

## Semantics and choices

- The file names no frame rate: the caller gives it (`import_edl(text, rate)`), and times keep
  that rate (§16.2). Drop-frame at a rate without 1001 in the denominator is reported.
- Events are in record order. Record time not covered on a track becomes a gap; the earliest
  record-in (often 01:00:00:00) becomes the IR timeline's start.
- A dissolve is two lines with one number: a zero-length cut on the outgoing reel, then the `D`
  line. The zero-length cut adds nothing; the `D` line becomes an `SMPTE_Dissolve` transition
  starting at the cut (in offset 0, out offset its length), then the incoming clip. The
  converter centres it on the cut and reports that.
- Wipes (`W###`) and keys (`K`, `K B`, `K O`) are kept opaque on their track and reported; the
  event still plays as a cut with its length.
- Reel `BL` (or `BLK`) without a source file is black: a gap of the event's length, not a media
  file (before 2026-10-20 it was reported as missing media). A dissolve or wipe to black is
  reported and becomes a cut to black.
- Clips of the same source file (or clip name, or reel) share one media reference.
- Lines before the first event other than TITLE/FCM are kept on the timeline and reported;
  lines after an event (comments, `M2`, `SPLIT:`, `AUD`) are kept on that event's clip.

## Limits (§18)

16 MiB of text, 100,000 events, 4,096 characters per line. A malformed event line refuses the
file with its line number. `fuzz_edl` (libFuzzer, ASan/UBSan) runs in CI.

## Fixtures

`tests/importers/fixtures/basic.edl` is written by hand to this structure (CRLF line ends, as
Windows editors write them): a cut, a dissolve, AA sound with an M2 line and a wipe after a gap.
No application's actual export has been checked yet.
