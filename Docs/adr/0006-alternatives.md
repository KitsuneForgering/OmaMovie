## Alternatives considered

- **libplacebo for color and scaling now.** Rejected for now (S6): no accuracy or cost gain for
  current stages; larger dependency surface; its C99 parameter macros need a C++ adapter.
- **Linear BT.2020 working space.** Rejected while no HDR or wide-gamut output exists: it adds
  conversions without a visible benefit; revisit with HDR.
- **Rejecting PQ/HLG sources.** Rejected: it would block common phone footage; a logged
  degradation keeps it editable.

[Return to the decision](0006-color-space-and-libplacebo.md)
