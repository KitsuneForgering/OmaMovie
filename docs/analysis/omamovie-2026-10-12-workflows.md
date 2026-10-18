## Professional workflow gaps, scoped to OmaMovie

Cost is a relative implementation estimate (small/medium/large), not hours or a
market forecast. Priority follows the recording/phone-video workflow; a feature
exists only to the extent indicated by the inventory. Each gate uses a real task.

| Gap and priority | Workflow unlocked; existing base | Cost / architecture risk | Decisive test; timing |
|---|---|---|---|
| Complete source-to-output edit — critical | Finish a dialogue/cutaway edit; marks, commands, playback and export exist | Medium / low if current commands stay authoritative | Same project through UI, reopened export with selected frames/samples; before v0.1 |
| Retimed clip audio policy — critical for audible speed edits | Avoid silent speech after a Speed action; TimeMap and mixer exist | Small to label/guard, large for pitch-preserving sound / medium sync risk | Tone pulses in preview and decoded export, undo/reopen; explicit policy before v0.1, stretch later |
| Reliable project/export recovery — critical | Resume interrupted hand-off; autosave, versions, relink and export journal exist | Medium / medium filesystem boundary risk | Inject writes/crashes, reopen installed app, inspect prior/pinned outputs; before v0.1 |
| Media organization/search — important | Find assets in a many-source edit; current library, Recordings and relink exist | Medium / low until persistent bins added | Time a named 100+ asset task and test search/reopen; after v0.1 unless task fails now |
| Portable collect/proxy association — important | Move an edit or seek costly originals; relink/cache and proxy pilot exist | Medium/large / medium identity and invalidation risk | Move package with originals unavailable; compare decoded output; after v0.1 pilot |
| Scopes/color management — important for graded hand-off | Judge and deliver a defined SDR/HDR target; SDR pipeline/LUTs exist | Large / high color-contract risk | Independent vectors plus display/export targets on real hardware; after v0.1 |
| One interchange format — important only for a receiver | Hand off cuts to a named NLE; native DTO and `oma-project` exist | Medium/large / high semantic-loss risk | Export → external import → re-export/import semantic comparison; after v0.1 pilot |
| Audio stems/mix controls — nice-to-have until delivery task | Separate dialogue/music; clip mix/EQ/fades exist | Medium/large / medium routing risk | Decoded stems recombine to program under declared tolerance; after v0.1 pilot |
| Multicam/nesting/full HDR suite — unnecessary for current v0.1 direction | Complex multi-camera/reusable/HDR delivery tasks not established | Large / high model, decoder and color risk | Define task and workload before a design; post-v0.1 only |

Large projects require a measured use case, not an automatic database or elaborate
bin hierarchy. Current 5,000-clip UI and warm project measurements are useful but
do not establish 10,000-clip multilane or three-hour production readiness. Keep
smoke, stress and performance runs separate. The listed estimates and priorities
should change when a concrete editor/receiver task supplies contrary evidence.

[← Assessment](omamovie-2026-10-12.md)
