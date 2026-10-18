## Local documentary sources

[Environment snapshot and hashes](evidence/2026-10-03-local.txt) record FFmpeg, Qt, Mesa,
Omarchy package labels, the different installed Omarchy version-file label and inspected
script/header identities. They are observations of this machine, not upstream provenance.

- `hwcontext_vulkan.h`: queue fields, semaphore values and locks (installed libavutil 61.1.101).
- Omarchy scripts: theme resolution/swap, capture directory/encoder intent, GPU package
  selection; Lua window rules and base package list. Source inspected, installer not run.
- Repository baseline `2f6abf3`: `libs/gpu/src/device.cpp`,
  `libs/media/src/vulkan_device.cpp`, `libs/compositor/src/color.cpp`,
  `tests/compositor/test_compositors.cpp`, `tests/support/lsan.supp`, public APIs/file layout.
  Code inspection is distinct from passing tests and performance measurement. The final
  existing debug-suite run is retained separately in [test output](evidence/2026-10-03-make-test.txt);
  it does not complete the proposed pilots.
- S1/S2 and implementation-plan timing paragraphs: previous experiment reports; not rerun.
  Procedures exist, but raw output, repetitions and provenance are incomplete in the docs.

## Failed access and exclusions

Apple Final Cut magnetic-timeline URLs attempted in this review, the CapCut `/resource/speed-curve`
URL and a Microsoft Windows Essentials URL returned errors. They supply no evidence.
CapCut's general guide provided a separate accessible speed-curve passage.

The original NVIDIA speed-ratio/forum, CapCut policy/pricing commentaries, review aggregators,
Movie Maker/iMovie histories and speculative future release details were not adopted as
verified evidence. This register does not imply every old link was checked or every product
installed. Vendor/help sources share their organization's lineage. No independent comparative
benchmark or representative user study was performed or found decisive in this bounded audit.

[Return to sources.md](sources.md)
