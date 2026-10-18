# Edit source navigation (bounded)

Select one graph node with a Path/PaintedPath default output, or one artwork composition, then use **Inspector → Edit source…**. Choose an explicit Text or Curve candidate. The composition's designated Output is the artwork search root. Selection, hovered Viewer and Viewer bindings are independent.

The Inspector shows original-stage fields, not inverse-transformed final coordinates. Curve reuses the existing owned-point panel, including explicit Browse all, stable point selection and pagination. Text reuses existing text controls. Navigation does not expand a collapsed graph card, rebind either Viewer, change Output, duplicate shared sources, or add an Undo entry. Shared uses continue to follow ordinary edits.

## Search contract

`source-navigation.js` is a pure, evaluation-free query. It follows only known geometry inputs:

- Output, Fill, Stroke, Text to Path, Transform, Path Remap: `path`
- Join Geometry: stable `joinInputKeys` order; empty spare slots are skipped
- Boolean: A then B
- Bypass: only the existing supported geometry identity contract
- Text and Curve: terminal content sources

Breadth-first traversal deduplicates source identities, preserves shortest representative routes and stable socket order, checks actual source ports and composition membership, and stops at 256 edges. Missing inputs, cycles, non-geometry ports and unsupported nodes are explicit incomplete-search diagnostics. Unknown modifiers and Multi Path are intentionally unsupported; no former Text is inferred from baked paths. Text Position connections, expression refs and Text path attachments are not content-source routes.

Candidate counts are distinct source nodes. The separately labelled **saved direct geometry inputs (nodes), same composition** count includes all saved geometry sockets, including other branches and bypassed/inactive owners. It is neither a visible-instance count nor a claim that all consumers contribute to the current root. Numeric refs and attachment roles are excluded from this count. Every candidate, even a single result, requires explicit selection.

Opening/cancelling the inline picker changes no graph selection or camera. A changed document/revision refreshes candidates and requires another choice. Changing the selected root dismisses the old picker. Lock permits navigation; existing Session command guards reject edits.

## Return and interaction boundaries

Back keeps one editor-local ticket. Later source trips retain the first origin. It restores surviving node/artwork IDs, graph zoom and world center (browser-clamped for the current viewport), and Inspector scroll. Missing return nodes are not recreated. If the origin composition is gone, the current composition remains and selection is cleared. Open/New discards the ticket even if the new document reuses IDs. Back never reverts edits, Undo history, explicit Viewer changes, or Artwork camera settings.

This first slice deliberately narrows the full return proposal:

- Point/character/remap/guide selection is restored only if the exact immutable document object still matches the origin. After any committed edit it is cleared, rather than guessing correspondence. Ordinary Undo can return to that exact object.
- Focus returns to the Source editing heading, rather than promising exact prior field/section restoration. Inspector pixel scroll is retained; resize may clamp it.
- Field controls must be explicitly left before navigation, including a currently focused clean field. Pointerdown guards run before blur can commit a numeric/text draft. IME, changed text drafts, rename, Pen, gesture/wire, canvas text, prepared variable actions, layout drafts and open modals block navigation without committing/cancelling them.
- Escape inside the source picker cancels only that picker. Back is an explicit button. Existing editor composition and Escape priorities remain first.

No source-specific canvas, final-output ghost handles, generated-point editing, inverse transform engine, schema, persistent source index or Viewer binding API changes are included.

## Evidence boundary

Focused pure-query and Happy DOM tests use synthetic AB/Text and small Curve graphs. The bundled DejaVu Sans fixture's existing license is in `public/fonts/LICENSE-DejaVu.txt`; no font or private artwork was added. Tests cover navigation state invariants, one-edit/one-Undo, stale lists, locked inspection, missing origins, replacement epochs, pointer guards, and picker Escape. These are not real-browser focus/IME/scroll, visual, Windows, EXE or full-suite acceptance.
