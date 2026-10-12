## Conclusion and question

For OmaMovie's short Omarchy recordings and phone videos, the next core visual
operation worth piloting is a **region mask**: opaque cover for hiding an area,
with optional local blur or highlight. A **focus/zoom preset** should reuse the
transform keys already present. A simple **chroma key** may follow the planned
PiP overlay. Stabilization, video denoise and automatic tracking need footage
and performance pilots first. A marketplace can start as a curated catalog of
editable presets; executable third-party effects require a separate host contract
and compatibility gate. These are priorities to test, not measured user demand.

The baseline already has color, CDL/curves/LUTs, filter looks, blur/sharpen,
vignette, crop, transform keys, transitions, audio EQ, FFT noise reduction and
peak normalization ([plan](../implementation-plan.md)). Compare candidates on
task completion, output correctness, undo/save/export, preview cost, trust and
the simpler manual alternative. Hypotheses: selective effects help recordings
more than additional whole-frame looks; editable presets reduce effort; only
some tasks justify third-party code. A representative task study showing the
opposite would change the ranking.

[Return to post-production-effects.md](post-production-effects.md)
