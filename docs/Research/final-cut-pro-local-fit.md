## Foundation and fit to this repository

The relevant foundation is an explicit timeline state model: time is rational,
clip identities survive edits, and a command must preserve declared invariants
or fail atomically. [ADR-0002](../adr/0002-time-representation.md) defines the
time representation; [the timeline API](../../libs/timeline/include/oma/timeline/edit.hpp)
states that ripple moves later clips **on the same track only** and exposes
transactional commands. These are verified local contracts. It is an **inference**
that a visual “connection” needs a persisted anchor relation rather than a
one-off UI group: otherwise save/load, undo/redo, and edits outside the UI can
break the relation. The exact anchor policy remains an assumption to resolve.

Define a minimal anchor as `(dependent clip ID, primary clip ID, offset in
sequence ticks)` only after deciding what split, trim, delete, speed change,
locked tracks, and movement across the anchor boundary mean. A surviving
anchor must refer to an existing primary clip. A failed operation should leave
timeline and history untouched; a successful operation should be one undo entry.
Do **not** infer Apple's internal representation from its visible behavior.

OmaMovie's current [implementation plan](../implementation-plan.md) already
includes M7 autosave/relink, M8 proxies and M9 FCPXML evaluation. The new issue
is order: anchor semantics must be settled before the native format is frozen.
The [README](../../README.md) says projects cannot yet be saved or exported, so
the immediate recovery gain is M7 durability, followed by versions only if the
mistake-recovery task justifies them.

[Return to final-cut-pro.md](final-cut-pro.md)
