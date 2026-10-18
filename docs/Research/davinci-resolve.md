# Resolve: versioned Linux constraints and product hypotheses

> Reviewed on 2026-10-03 with skeptical-research. This is non-normative research.
> Source metadata and access limits: [source register](sources.md). Decisions and open
> validation gates: [audit](skeptical-review.md). Product descriptions are not user studies.

## 1. Conclusion

The inspected codec matrix supports a bounded Linux codec-accessibility opportunity.
It does not establish that only NVIDIA can run Resolve, that ALSA prevents use on PipeWire,
or that the market between consumer and professional Linux editors is empty.

## 2. Version and source boundaries

The [current Blackmagic product page](https://www.blackmagicdesign.com/products/davinciresolve/techspecs)
identifies Resolve 21 and editing, Cut, Color, Fusion and Fairlight contexts. It is product
information, not a benchmark or specification of private engine internals. This audit did
not verify the former 21.1/MCP/API counts or release chronology; those claims are withdrawn.

Codec evidence below comes from the **Resolve 20, July 2025** matrix. Do not silently apply
it to Resolve 21, different editions, containers or future updates.

## 3. Linux codec evidence

[Blackmagic's supported-codec PDF](https://documents.blackmagicdesign.com/SupportNotes/DaVinci_Resolve_20_Supported_Codec_List.pdf),
printed pp. 11 and 14, under Rocky Linux 8.6 (CUDA):

| Entry inspected | Supported conclusion within that table |
|---|---|
| H.264/H.265 mov/mkv/mp4 | Decode marked Studio only, acceleration on NVIDIA; encode Studio + NVIDIA |
| AAC / m4a | Decode/encode marked unavailable |
| Linux section title | Documents a CUDA configuration; does not prove all other GPU APIs impossible |

This is documentary analysis, not a local Resolve installation test. The old blanket
"NVIDIA only officially supported" and ALSA/PipeWire exclusion are not inferred from a codec
matrix. Edition-specific media restrictions must be rechecked before public comparisons.

## 4. UX and project inference

Cut and Edit suggest that the same product can present editing in different contexts.
For OmaMovie a minimap versus single zoomed timeline is still a testable preference; pages
or a node/layer view do not validate our exact layout or render-graph representation.

Choose file-based project persistence for portability/recovery requirements. Database
storage can also provide transactions and portability via export; no storage form is
inherently unreliable. A database/workspace workflow does not prove that a file design is
universally better. `.drp` import remains uninvestigated, not an accessible-format promise.

## 5. OTIO and fidelity

[Continue in davinci-interchange.md](davinci-interchange.md)
