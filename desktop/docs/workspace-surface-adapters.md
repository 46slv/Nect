# Workspace surface registration

`WORKSPACE_SURFACES` is the static declaration of existing drawer controls. The workspace controller validates the full declaration and trusted adapter mapping before touching DOM or preferences. `main.js` composes the existing owners into those adapters; the controller remains view-only and does not write the document or history.

The Assets adapter keeps the existing singleton under `document.body` and aligns it to the drawer's fixed slot. Drawer close, panel switch, viewer suspension and drag retreat hide or suspend that same element; they do not dispose the Assets owner. The Assets owner's own Close/Escape keeps its existing disposal and epoch behavior. Existing Shared Colors and selector-based panels remain legacy bindings, including their existing event and Session ownership.

The legacy Colors binding attaches the existing element after the Assets slot and synchronizes its visibility; the Alignment binding synchronizes its toolbar pressed state. Adapter refresh runs in reverse declaration order so these updates precede the Assets slot measurement. This preserves the existing order; it is not a general layout policy.

This is a fixed-placement adapter, not relocation, a profile system, independent left/right drawers, or a new feature controller. Existing cross-side exclusivity, preference format and workspace geometry remain in force. Actual browser/native, active IME and in-flight asset integration remain separate acceptance work.

Each existing adapter declares its fixed supported side; the controller checks the declaration against that capability before it reads preferences or attaches DOM. This is not a movable-placement policy: a different side is rejected. Assets owns its body portal and in-flight work; Shared Colors owns its input/IME draft, app-level events and Session envelope; the host owns focus, pointer and drawer visibility. Future relocation would need each owner to expose an active-interaction decision, defined suspend/resume and focus restoration, and failure rollback before moving. The fixed-side check does not establish safety for real IME, pointer capture or asynchronous file work.
