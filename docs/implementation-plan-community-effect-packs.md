### Conditional pilot — Community effect packs and curated marketplace (after M7/M8)

The maintainer requested a place for others to publish production/post-production
effects (2026-10-04). This is an explicit future scope direction under
`CLAUDE.md` §2; the public extension architecture still needs an ADR. An installed
package and a curated catalog are separate: the core registry reads local packages,
while an optional catalog only helps people find and install them. Editing, preview,
save/reopen and export work offline, without an account or marketplace service.
Start with community **data-only packs** of presets built from native operations,
so applying one creates editable project content without loading external code.

- [ ] After the effects ADR, define a versioned package manifest with stable package and preset IDs, author/license, contents, preview, compatible OmaMovie and effect-definition ranges, asset hashes and bounded file sizes. Keep package installation separate from project instances: the project stores the identities and values needed to render or explain a missing dependency. Pilot local install/update/disable, project save/load and missing-pack recovery before adding a catalog. Show source, version and contents before installation; preserve a working prior version if an update fails.
- [ ] Compare installing a pack with manually reproducing the same effect on target tasks. Measure discovery, completion, corrections and export fidelity; expand the catalog only if it improves that workflow.
- [ ] Only after data-only packs and the core contract prove useful, write a separate executable-host ADR and prototype one isolated out-of-process effect before accepting code submissions. Specify parameters, color/alpha/time, threading, bounded resources, deterministic export, version pinning, crash/timeout behavior, permissions, update rollback and missing-effect reporting. Measure CPU↔GPU transfer and p95/p99 preview time; prove any Vulkan handoff separately. Do not load downloaded native code into the editor by default. Evaluate OpenFX against this contract rather than promising universal plugin compatibility.
- [ ] Publish submission/review rules and a versioned compatibility test suite before listing third-party binaries. Catalog review is not a sandbox or a guarantee of safe execution.

**Research**: [effect research and marketplace alternatives](Research/post-production-effects.md).

[← Implementation plan](implementation-plan.md)
