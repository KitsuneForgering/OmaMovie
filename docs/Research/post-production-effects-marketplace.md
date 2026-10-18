## Marketplace: staged design hypothesis

**Stage 1 — curated preset catalog.** Third parties submit a versioned manifest,
license, preview, description and parameter values built only from OmaMovie's
existing effects/commands. The app shows author, version and contents before a
user installs a pack. Install per user, explicitly; keep versions and hashes so
a saved project can identify its dependencies. A preset creates editable native
parameters and remains usable if the pack later disappears. A repository of
static manifests and downloadable assets is enough for a pilot; no account,
payment service or code loader is needed to discover whether sharing is useful.
For recording-time presets, generate an ordinary edit on imported footage first;
integration with the Omarchy recorder needs its own lifecycle and permission
contract. The catalog may carry both production-style presets and post-production
looks, but must say when each is applied.

**Stage 2 — executable effects, only after a host pilot.** A third-party effect
must have a stable ID/version, declared inputs/outputs, color/alpha/time domain,
parameters and defaults, deterministic save/undo/export semantics, supported
platform/GPU paths, resource limits and a missing-effect behavior. Test one
out-of-process CPU effect on representative footage, with timeout/crash
recovery and a visible bypass/error state, before specifying a public SDK.
Do not load downloaded native libraries into the editor process by default:
they can crash it or access its data. A process boundary costs frame transfers;
measure that cost before claiming real-time preview. Any GPU handoff needs a
separate Vulkan ownership/synchronization proof. The catalog must distinguish
presets from executable code and disclose the latter's author, source, permissions,
update path and review status. Review is a distribution policy, not a security
guarantee. Failed updates must leave a usable previous version; projects pin an
effect identity/version and report missing or incompatible effects before export.

**Alternative considered:** [OpenFX 1.5.1](https://openfx.readthedocs.io/en/main/Reference/ofxImageEffectAPI.html)
is an established image-effect API with parameters, clips and host suites.
Its [loading model](https://openfx.readthedocs.io/en/main/Reference/ofxCoreAPI.html)
loads a binary into the host, and its [rendering model](https://openfx.readthedocs.io/en/main/Reference/ofxRendering.html)
documents CPU, OpenGL and other GPU paths; the reviewed API does not provide
OmaMovie's current Vulkan image contract or process isolation. Adopting a full
OFX host solely to launch a marketplace is therefore unjustified now. Revisit
with an actual third-party effect and a measured CPU-transfer/host prototype.
[Blender Extensions](https://extensions.blender.org/about/) demonstrates an
online catalog with install confirmation and submission review, but its product
policy and implementation are not evidence that the same marketplace will work
for OmaMovie.

[Return to post-production-effects.md](post-production-effects.md)
