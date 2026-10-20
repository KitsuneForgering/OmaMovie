# Same-machine comparison with melt — 2026-10-20

OmaMovie `68d7c2d` (Release; hardware decode where validated, proposed ADR-0022) against
melt 7.42 (Kdenlive 26.08.2's engine) on one i5-1135G7 / Iris Xe laptop: FFmpeg 9.0.2, Mesa
iHD 26.3.5, `intel_pstate` `powersave`, EPP `balance_performance`, `balanced` profile, AC.
Three alternated rounds per cell (`tools/bench/cross-editor/run.sh 3 … whole|cuts`,
`layers.sh 3`); every output checked by FFmpeg alone. Raw reports:
[whole](../Research/evidence/2026-10-20-cross-editor-whole.txt),
[cuts](../Research/evidence/2026-10-20-cross-editor-cuts.txt),
[layers](../Research/evidence/2026-10-20-cross-editor-layers.txt).

| Scene (1080p23.976) | Route | OmaMovie wall s, median (range) | melt wall s, median (range) | CPU s, OmaMovie / melt |
|---|---|---|---|---|
| Whole 20 s source | VA-API H.264 | 4.99 (4.98–5.03) | 6.63 (6.04–6.74) | 7.0 / 16.8 |
| Whole 20 s source | libx264 | 16.1 (12.5–17.5) | 15.8 (15.1–16.7) | 104 / 112 |
| Five reordered cuts | VA-API H.264 | 3.64 (3.47–4.68) | 4.20 (4.19–4.34) | 4.8 / 15.0 |
| Five reordered cuts | libx264 | 9.33 (9.32–9.44) | 10.33 (9.92–12.23) | 55.3 / 66.7 |
| Three layers, two scaled | VA-API H.264 | 4.17 (3.85–4.37) | 15.5 (14.9–16.3) | 5.6 / 40.2 |
| Three layers, two scaled | libx264 | 4.94 (4.86–5.56) | 17.7 (17.5–18.2) | 21.7 / 54.8 |

**Correctness (all 36 exports passed).** Frame counts exact (479, 256, 240); every frame of
the cuts scene matches its planned source frame; output bytes identical across rounds for
each editor and route. Worst luma PSNR against the source: whole/cuts OmaMovie 44.3 / 41.1–42.0
dB, melt 45.0–45.1 / 41.3–42.2 dB; layers against FFmpeg's scale+overlay: OmaMovie 41.4–41.9,
melt 44.2–44.5 dB. Audio correlates at zero lag in both; melt's stereo is 0.68–0.70 of the
mono source (its −3 dB equal-power upmix), OmaMovie's 0.98.

**Reading.** On the libx264 route x264 dominates both editors; the whole-file ranges overlap
and the cell is a tie. With VA-API OmaMovie used 2.4–3.1× less CPU and finished 13–25%
sooner on plain and cut scenes. The layer gap (7× CPU) is large because melt's `affine`
composites on the CPU;
melt's picture is about 3 dB closer to FFmpeg's reference, consistent with OmaMovie's
linear-light scaling (ADR-0006).

**Not comparable / not measured.** Kdenlive's own compositing (`qtblend`/movit): a melt
`qtblend` reconstruction drew only the background, so no number. Playback, seek and
interactive latency: no shared task yet; melt's `sdl2` consumer exposes no dropped or
skipped frame count to set against OmaMovie's player-drop audit. Other machines, other GPU vendors, FilmCraft's
published figures: different hardware or method, so outside this table. Three rounds give
ranges, not confidence intervals; no cell supports a general speed claim about either editor.
