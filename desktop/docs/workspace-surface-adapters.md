# Workspace surface registration

`WORKSPACE_SURFACES` is the static declaration of existing drawer controls. The workspace controller validates the full declaration and trusted adapter mapping before touching DOM or preferences. `main.js` composes the existing owners into those adapters; the controller remains view-only and does not write the document or history.

The Assets adapter keeps the existing singleton under `document.body` and aligns it to the drawer's fixed slot. Drawer close, panel switch, viewer suspension and drag retreat hide or suspend that same element; they do not dispose the Assets owner. The Assets owner's own Close/Escape keeps its existing disposal and epoch behavior. Existing Shared Colors and selector-based panels remain legacy bindings, including their existing event and Session ownership.

This is a fixed-placement adapter, not relocation, a profile system, independent left/right drawers, or a new feature controller. Existing cross-side exclusivity, preference format and workspace geometry remain in force. Actual browser/native, active IME and in-flight asset integration remain separate acceptance work.
