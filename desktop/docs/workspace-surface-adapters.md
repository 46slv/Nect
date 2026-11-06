# Workspace surface registration

`WORKSPACE_SURFACES` is the static declaration of existing drawer controls. The workspace controller validates the full declaration and trusted adapter mapping before touching DOM or preferences. `main.js` composes the existing owners into those adapters; the controller remains view-only and does not write the document or history.

The Assets adapter keeps the existing singleton under `document.body` and aligns it to the drawer's fixed slot. Drawer close, panel switch, viewer suspension and drag retreat hide or suspend that same element; they do not dispose the Assets owner. The Assets owner's own Close/Escape keeps its existing disposal and epoch behavior. Existing Shared Colors and selector-based panels remain legacy bindings, including their existing event and Session ownership.

The Colors binding mounts its view after the Assets slot and synchronizes its visibility; it can still attach an existing element for legacy callers. The Alignment binding synchronizes its toolbar pressed state. Adapter refresh runs in reverse declaration order so these updates precede the Assets slot measurement. This preserves the existing order; it is not a general layout policy.

The approved Layers and Inspector prototype declares a fixed co-visible `editing`
group in `WORKSPACE_SURFACES`. Both sections keep their existing owners and scroll
independently in the right drawer; the selected rail denotes the focus target and
either legacy right-side preference opens the stack. The group does not create a
movable docking system. A right-drawer composition blocks rail, Viewer and sibling
section transitions before pointer blur; after composition ends the normal input
owner remains responsible for its existing commit behavior. Physical IME and native
WebView2 still require separate verification.

The same fixed group declares its primary Layers split. The host provides the
separator and stores only its view preference; it never writes the Session. A
cancelled gesture or replaced host discards the preview, while a committed size
is bounded by the available viewport and restored after structural render.

One co-visible right-side declaration may offer a `toolsShelf`. Its optional
view preference moves the existing creation rail into that section while the
group is visible, and returns it to the canvas edge otherwise. No additional
tool owner or authored state is created; this is not arbitrary surface placement.

Production supplies the Shared Colors controller's view factory to its fixed adapter. The controller's existing HTML projection creates the view for each structural render; the adapter mounts it after the Assets slot inside the app. The controller retains its delegated events and Session ownership. This does not make the view persistent or movable.

These are fixed-placement surface adapters, not a general profile system, independent left/right drawers, or a new feature controller. Existing cross-side exclusivity, preference format and workspace geometry remain in force. Browser evidence covers the recorded paths; physical IME, native WebView2 and in-flight asset integration remain separate acceptance work.

Each existing adapter declares its fixed supported side; the controller checks the declaration against that capability before it reads preferences or attaches DOM. This is not a movable-placement policy: a different side is rejected. Assets owns its body portal and in-flight work; Shared Colors owns its input/IME draft, app-level events and Session envelope; the host owns focus, pointer and drawer visibility. The Colors adapter consults that owner while composition or a color pointer is active, and its input guard protects the focused draft before pointer blur and Viewer activation. When a name change occurs during a pointer transition, the existing deferred-render path keeps the destination alive until the click finishes. Synthetic-composition and pointer behavior has been checked in Windows Chrome; physical IME and native WebView2 remain unverified. This bounded check does not establish general surface relocation or asynchronous file-work safety.

`attach` checks the new root and its required canvas, right panel and tools before detaching the current host. An incomplete target leaves the previous host and queued restoration in place. This does not reconstruct DOM that a caller already removed, or implement arbitrary surface relocation or active-edit migration.
