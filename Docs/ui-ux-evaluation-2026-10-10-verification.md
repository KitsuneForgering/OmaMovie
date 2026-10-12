## Verification

The GUI smoke drives mouse edits, keyboard menus, palette search and execution, undo/redo,
contextual title, picture and sound branches, a library resize, and layout geometry at 820 and
640 px. `make test`
checks the headless editing and media layers. The smoke's `grabWindow()` image is useful for
rough visual review, but its Qt/Vulkan capture path can disagree with the measured item geometry
and it does not establish screen reader or real user task performance.

[Return to ui-ux-evaluation-2026-10-10.md](ui-ux-evaluation-2026-10-10.md)
