## Architecture and product decision

The module boundaries are mostly appropriate for a desktop editor: Qt stays in the app,
and the timeline model does not depend on FFmpeg or Vulkan. The remaining architecture
work should serve concrete outputs: one shared, deterministic composition contract for
preview and export; explicit output color/audio settings; lifecycle rules across Qt,
decoder and compositor; and recoverable project state. A separate service, general plugin
API, alternate project database or importer framework would add maintenance before this
workflow is proven.

The relevant engineering lenses are timeline invariants and algorithmic complexity for
large edits; real-time scheduling/backpressure for audio and preview; durability and
recovery for projects; and end-to-end quality scenarios for the editor's user-visible
claims. The current tests cover many library invariants, but a green library suite cannot
establish delivered movie correctness, crash recovery or smooth Qt presentation. Those
are the next integration gates.

Recommended order: (1) complete narrow export and independent output checks; (2) finish
save/reopen recovery with fault injection and state-machine tests; (3) run the M4 viewer
gate and long-form interaction gate, then optimize only the measured dominant paths.
The cost is a slower feature rollout, but it yields a usable editing loop and avoids
optimizing or expanding an unverified one.

[Return to project-analysis-2026-10-04.md](project-analysis-2026-10-04.md)
