# MLT timewarp (Kdenlive speed change) against melt — 2026-10-20

**Classification:** INTEROP / correctness. Same machine, same document, each output read by an
independent FFmpeg decode, never by the editor that wrote it.

**Before:** a `timewarp` producer (how Kdenlive and melt save a speed change) imported its
`resource` (`2:path`) as a file name, so the clip was reported as *media file not found* and
left as a gap, although the file existed.

**Document:** `tests/importers/fixtures/melt_timewarp_23976.mlt`, written by melt 7.42 with
`melt timewarp:2:SRC in=1 out=5 timewarp:0.5:SRC in=8 out=13 SRC in=16 out=21 -consumer xml:`
over the 24-frame 320×180 23.976 fps fixture `tests/fixtures/generated/h264_23.976fps.mp4`.

**melt's render** (`melt melt_timewarp_23976.mlt -consumer avformat:… vcodec=ffv1`), each output
frame matched to the nearest source frame on a 64×36 gray thumbnail: 17 frames,
`2 4 6 8 10 | 4 5 5 6 6 7 | 16 17 18 19 20 21`.

**OmaMovie** (import → save → reopen → software export, `tests/app/test_shared_speed.cpp`, same
matching): `2 4 6 8 10 | 4 4 5 5 6 6 | 16 17 18 19 20 21`. The 2× and 1× clips, every clip
length and the half-speed frames on whole source positions agree with melt. At half speed the
positions 4.5, 5.5 and 6.5 lie between two source frames: melt rounds them up; OmaMovie keeps
the frame already shown, so each source frame plays twice from the in point. The test asserts
OmaMovie's choice and documents melt's; neither is called correct.

**Mutation check:** placing the clip with a 1× time map instead of its speed fails the shared
test (1 failure of 129).

**Sound:** melt's `warp_pitch` defaults to 0 (pitch follows the speed, `melt -query
producer=timewarp`), the same rule as OmaMovie's retimed audio. `warp_pitch=1` (pitch
compensation) is reported, not imported. A speed whose audio rate is not whole at 48 kHz
(0.7×) plays silent in OmaMovie and now gets an import note (`speeds.mlt`,
`tests/app/test_import_timeline.cpp`); the audio itself was not compared with melt here.

**Not covered:** reverse (negative) speeds stay reported gaps; Kdenlive's `timeremap` link
(variable speed) is not read; no project saved by Kdenlive itself was available.

Tools: melt 7.42.0, FFmpeg n9.0.2, GCC debug build of the working tree on `82df20e`.
