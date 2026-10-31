# Workspace drawers (trial)

This is an integration candidate, not a statement of product acceptance. The
editor's job is artwork creation and graph editing; the Artboard and Node Graph
remain the primary working surfaces. The existing creation rail belongs to the
upper surface. Side drawers are secondary overlays bounded by the Artboard
viewport, and the lower graph occupies the workspace width.

## Ownership and interaction

`workspace-panels.js` owns only drawer selection, focus, portal geometry and
view preferences. Its small panel definition list binds existing owners: Assets
and project Shared Colors on the left; Layers, Alignment and Inspector on the
right. One panel per side can be open. Selecting the same trigger closes it;
selecting another switches that side. This routing is a reversible trial.

The document and Session own artwork, selection targets, commands, validation,
preview and history. Moving, showing, hiding or restoring a panel is not an edit.
An Asset drag retains the library's live DOM and original Session capture while
briefly retreating the left overlay. The canvas transform must not change. A
successful placement uses its existing command and one Undo; cancellation or a
stale result does not commit. Color controls keep their existing live gesture
and one-Undo behavior; there is no new color-to-canvas drag capability here.

Drawer preferences use a separate view-only key. They do not serialize input
focus, transient drag or authored document state. Invalid or older preferences
must not block editing. Native modal dialogs and in-progress text editing retain
Escape precedence. A closed panel must not expose interactive hit surfaces.
In Viewers mode the left Assets/Color overlay is temporarily hidden and inert,
while the right Inspector remains available for viewer assignment; returning
to Artwork restores the retained left panel. A minimum upper-pane height keeps
the edge controls usable at a short split while preserving the graph below.

## Design and evidence

The accepted edge silhouette is a functional navigation cue, not a generic
icon rail; stable hit areas and visible states fit inside it. Cool neutral
surfaces, restrained focus/selection accents, dense but readable controls and
persistent spatial orientation support the creative workflow. Familiar desktop
shortcuts and the existing semantic commands take priority over decorative
motion. Narrow windows constrain secondary overlays rather than changing the
Artboard/graph layout; reduced motion does not remove state feedback.

Verify both fit and manual canvas transforms, unchanged graph geometry,
independent side switching and dismissal, native asset drag/cancel and stale
requests, focus/IME and modal Escape, and same-condition rendered states. A
passing browser test is not Windows native GUI or commercial-font acceptance.

Interface typography is centralized in `--ui-font-family` and UI weight-role
tokens. These style controls and labels only; changing the stack does not edit
artwork Text, font sources, project serialization or history. The selected UI face is IBM Plex Sans JP Light (300), with Regular (400)
for small functional labels, using the original complete hinted WOFF2 files.
The fallback stack remains available. No user font file or new font-selection
feature is included in this trial.

The bundled files and unchanged SIL OFL 1.1 notice come from
[IBM/plex](https://github.com/IBM/plex) commit
`71f558992315a12ca1f06a2215d57962a1992162`, under
`packages/plex-sans-jp/fonts/complete/woff2/hinted/` and
`packages/plex-sans-jp/LICENSE.txt`. Light SHA-256:
`4d40f9921e3467d01acf4d356b19754b1a80c65e24841dc79644134233f6002d`;
Regular SHA-256:
`ec27e9b75cd90a6f7abe96ede49d00b151eb5b1c2c886bfe9b4fc845f40be1ff`.
Browser loading and rendered glyph quality remain separate verification steps.

The full Node test command limits concurrency to four workers. Windows CI runs
`41153528942` and `41156675209` encountered process allocation failures under
the unbounded default; this limits peak contention without skipping tests or
changing their assertions. A subsequent full run is required as evidence.
