## Next design and engineering gates

1. **Direct manipulation:** framing already supports moving, scaling, rotating and crop edges.
   Add on-canvas text editing, then evaluate masks and tracking only with a supported backend.
2. **Command coverage:** continue moving existing menu and drawer operations into registered
   actions. A future agent interface should call the same validated editing commands through an
   explicit API; it should not automate UI widgets.
3. **Navigation:** test a short edit with mouse and keyboard at 640, 820 and 1600 px, including
   larger text and a light Omarchy theme. Keep the existing remappable shortcuts; evaluate
   optional keymap profiles only after checking conflicts with text fields and Hyprland.
4. **Professional features:** roll, slip, slide, markers, nested sequences, masks, tracking,
   scopes and broader export controls need model and pipeline support before presenting them as
   working menu items. Group new effects by intent and reveal precision controls in the relevant
   drawer.
5. **Accessibility:** verify a screen reader, keyboard focus in every popup and drawer, larger
   text, contrast and target sizes. The current smoke covers focus paths, not screen readers.

Keep unavailable features out of active menus. A command may be discoverable only when the
selected object and the backend can execute it. This preserves professional capacity as the
backend grows without promising a control that does nothing.

[Return to ui-ux-evaluation-2026-10-10.md](ui-ux-evaluation-2026-10-10.md)
