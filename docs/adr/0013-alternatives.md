## Alternatives considered

- **Bézier or sampled speed curves.** Need numerical integration and inversion, so
  bounded approximation and a rounding policy per evaluation; piecewise-linear speed covers
  the ease-in/out presets the creator research asks for with exact integrals.
- **Reverse as a clip flag.** Cannot express "forward, then backward" boomerangs and
  duplicates what a negative-speed segment already says.
- **Derive duration from the map.** Impossible for freeze (zero displacement), as the
  review note says; the stored duration stays the source of truth.

[Return to the decision](0013-clip-time-maps.md)
