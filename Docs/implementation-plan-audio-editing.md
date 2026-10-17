### Audio editing ahead of order (2026-10-03)

The maintainer chose to finish audio editing and start video effects before M7
(`CLAUDE.md` §24 order), with persistence and export still required for v0.1.
Done: audio lanes (import sound, place, move between lanes, trim without ripple),
waveforms (`playback::compute_waveform`, 10 ms peaks and RMS, session-only until
ADR-0009 decides the cache), detach audio for J/L-cuts (`edit::detach_audio`,
`Clip::audio_detached`), Volume "More" (equalizer presets, noise reduction,
peak normalize). Not done: linking lane clips to the storyline clip they belong to
(sync-locked ripple), loudness (LUFS) normalization, speed-changed clip audio.
Prepared for more clip audio effects (2026-10-04): `audio::Effect` (in-place planar
processing without allocation, reset on seek) and `audio::EffectChain` (slots that keep their
state across reconfiguration); the equalizer is the first effect and playback builds each
clip's chain in one place (`libs/playback/src/clip_effects.cpp`). Noise reduction stays in
the decoder (FFmpeg `afftdn`).

[← Implementation plan](implementation-plan.md)
