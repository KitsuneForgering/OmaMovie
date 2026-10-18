# ADR-0009 — Cache strategy

**Status: Accepted (2026-10-06).** Required by CLAUDE.md §15 before the first disk cache
(thumbnails and waveforms, M6/M7).

## Context

Thumbnails and waveforms are computed again for every session: thumbnails go to a temporary
folder, waveforms live in memory. Opening a long project re-decodes every item. The Vulkan
compositor already keeps a pipeline cache file per device and driver in
`$XDG_CACHE_HOME/omamovie/`. Decoded frames stay in memory (`FrameSource`'s bounded history).
CLAUDE.md §15 fixes the rules: the cache is never a source of truth, keys come from the inputs
(media identity + parameters + algorithm version), limits are configurable with eviction, disk
caches live under `$XDG_CACHE_HOME/omamovie/`.

## Decision

- **One disk cache store in `libs/base`** (`DiskCache`, no Qt): opaque byte entries under a
  directory, one file per entry, named by a 128-bit hash of the entry's **full key string**.
  The file starts with a header holding that key, the payload size and a checksum; a read whose
  key, size or checksum differs is a miss (hash collisions and torn files are harmless).
  Writes go to a temporary file in the same directory and are renamed into place, so a reader
  sees an old entry or a new one, never half of one; no fsync — losing an entry only costs a
  recomputation.
- **Keys** are text built by the caller from: the kind and algorithm version
  (`thumbnail/v1`), the media identity (absolute path, size, modification time and the
  project fingerprint's partial hash) and the parameters (size, time, channel count…). Any
  change to an input changes the key; there is no explicit invalidation.
- **Limits and eviction.** A size budget per store (default 512 MB, settable in Settings).
  After writes, a background pass removes the least recently used entries (by modification
  time, which a hit refreshes) until the store is under 90 % of the budget. Entries are
  independent files, so eviction never blocks readers.
- **Location.** `$XDG_CACHE_HOME/omamovie/<kind>/` (`~/.cache/omamovie/` without the variable);
  never next to the project. Deleting the folder loses nothing but time.
- **Kinds now.** Thumbnails (PNG bytes) and waveforms (the peak arrays). The pipeline cache
  keeps its own file (its key is the driver's own header). Decoded and rendered frames stay in
  memory caches with byte budgets; a GPU frame cache waits for a VRAM budget
  (`VK_EXT_memory_budget`) and is a later decision. Proxies are out of scope until M8.
- **No SQLite yet.** File names are the index; a SQLite index is justified only when listing or
  querying entries (e.g. per-project cleanup, statistics) becomes a measured need.

## Alternatives considered

- **SQLite with blobs.** One file and transactions, but large blobs in SQLite cost write
  amplification and a writer lock shared by job threads; file-per-entry needs neither.
- **Hash-only file names without a stored key.** Smaller files, but a collision or an old
  algorithm's file would be read as valid.
- **Caching next to the project.** Rejected by CLAUDE.md §15.

## Consequences

- Reopening a project shows thumbnails and waveforms without decoding when the media did not
  change.
- Many small files: fine for thumbnails and waveforms (one per library item); revisit for
  per-frame caches.
- The cache never needs migrating: a new algorithm version produces new keys, and old entries
  age out by LRU.
