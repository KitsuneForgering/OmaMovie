## 6. Adjustments and control drawer

**Decided 2026-10-04 (maintainer):** there is no permanent bar of adjustment icons. The
adjustments open from the clip's context menu (right click, `Shift+F10`/Menu key) or their
shortcuts; the menu lists only what applies to the clicked clip and marks with ● the ones
already active on it. While a drawer is open, a thin strip above it names the adjustment and
the clip and closes it ("Done" or `Esc`). The table keeps the planned adjustments; their icons
remain for menus and tooltips.

| Icon | Adjustment | Drawer controls (level 1) | "More" (level 2) | Release |
|---|---|---|---|---|
| ◐ | **Color** | Auto, exposure, contrast, saturation, temperature | Color wheels, curves, LUT | v0.1 basic, v0.3 advanced |
| ⬚ | **Crop and framing** | Fit / Fill / Crop; pan & zoom (Ken Burns) | Free rotation, numeric position | v0.1 |
| ♪ | **Volume** | Volume, fade in/out, mute | Equalizer, noise reduction, normalize | v0.1 basic |
| ⏱ | **Speed** | Slow / Normal / Fast, reverse | **Speed ramps with presets**, freeze frame | v0.2 |
| ✦ | **Effects** | Filters with preview thumbnails | Parameters, keyframes | v0.2 |
| ⧉ | **Overlay** (only on layers above the primary storyline) | Picture-in-picture, Side by side, Cutaway, Chroma key | Border, shadow, mask, blend | v0.2 |
| ⓘ | **Info** | Name, duration, source file, codec, decode path | — | v0.1 |

- The **drawer** opens above the viewer, pushing it down slightly (the viewer shrinks instead of being covered).
- **Library sidebar**: the top bar's library button (`Ctrl+1`) hides it in wide windows too,
  leaving the viewer the whole width, centered; in narrow windows it is an overlay.
- **"More"** expands the drawer. If the content is large (curves, keyframes), a **side panel** opens in place of the library, with "◂ Back to library".
- For titles the bar changes: **Text** (font, size, color, alignment), **Style** (presets), **Animation**.
- For transitions: **Type** and **Duration**.

---

[← UI design](ui-design.md)
