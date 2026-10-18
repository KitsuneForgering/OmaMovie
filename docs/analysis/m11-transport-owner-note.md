# M11 note: an owner for the transport? Lifetime, threading and coupling (2026-10-14)

Candidate 1 of the [M11 inventory](m11-inventory-2026-10-14.md): move the transport out of
`Session` (`togglePlay`, `shuttle`, `setSpeed`, `startPlayback`, `pause`, `adaptPreview`,
`onTick`). CLAUDE.md §13/§21 ask for the lifetime and threading impact before such a change.

## Threads: unchanged by an extraction

The threads already have owners. `AudioPlayer` runs its render pipeline on its own one-thread
`JobPool` and the PipeWire callback consumes its ring; `VideoScheduler` decodes ahead on its own
one-thread `JobPool` into a bounded queue (ADR-0003). Both document `start`/`stop`/`take` as
UI-thread calls, and `stop()` joins its producer. Everything the seven Session methods do —
speed, the `QTimer` tick, the clock, the frame target, adaptive preview quality, notices — runs
on the UI thread. Moving those methods and their state into another UI-thread object changes no
thread, queue, callback or synchronization: it is not a threading-model change.

## Lifetime: one constraint

`session.hpp` orders members so that `audio_`, the scheduler and `workers_` are destroyed before
what their producers use (the preview item's device, media paths, LUT tables), and the comments
say so. An owner object must keep that order: declared where `audio_` and `scheduler_` are now,
holding the same members, with no new references outliving `Session`.

## Coupling: the real cost

State used only by the transport (would move): `speed_`, `audio_`, `scheduler_`, `tick_`,
`tick_clock_`, `tick_ms_`, `tick_gap_ms_`, `clock_`, `play_from_frame_`, `adapt_shown_`,
`adapt_dropped_base_`, `warned_silent_`, `last_restart_ms_` (13).

Inputs it shares with editing (would cross the new boundary): the playhead and source-viewer
frame, source mode and length, the timeline snapshot and source timeline, media paths, LUT
tables, canvas size, ticks per frame/sample, frame rate, the preview item and its quality
setting, `setFrame`, `lastFrame`, `requestFrame`, notices and failure reporting (about 20).

An owner would need all of those as a constructor bundle or callbacks: a wide interface around a
small amount of logic, close to the parallel service layer M11 rules out.

## Recommendation

1. Do not move ownership now. Extract the part that is hard to test: the per-tick decision (which
   frame the clock asks for, whether playback has reached its end) and the adaptive-quality window,
   as pure functions with unit tests, leaving state and calls in `Session`. No lifetime or thread
   change at all.
2. Revisit an owner only if a second client needs the transport (for example source-viewer
   playback diverging from sequence playback), which would justify the interface.
