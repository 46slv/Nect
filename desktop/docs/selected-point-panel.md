# Selection-first node point controls

2026-10-17. Bounded UI improvement, not the proposed full Inspector redesign.

Curve, owned Multi Path, and generated Path Remap node cards now start with a
point count and Canvas selection guidance instead of eagerly creating every
point's numeric controls. Canvas anchor selection/Shift-toggle is reflected by
stable point ID in the node panel. Selected rows have a blue edge; the active
point has a gold edge and its row is opened/revealed inside the scroll area.

At most 24 point rows are created per node card. Larger selections have pages.
Explicit **Browse all points** provides paged access to every point, including
ambiguous overlapping anchors that cannot be picked directly on Canvas. The
point button in a row selects that stable ID (Shift toggles it), returning to
selection-first display. A new Canvas selection also returns to this mode.

The existing field renderer and Session commands still own numeric values,
locks, sources, expressions, handle types, deletion and undo. No core/schema or
geometry conversion changes were made. Undo/document replacement resets point
selection and browsing state as part of the existing transient-state reset.

Remap's checkbox-based batch selection remains independent of Canvas point
selection and is explicitly labelled **batch checked**. Its delta fields appear
when at least one batch point is checked. Checkbox state survives point-panel
filtering and paging. Generated topology editing is not added; typed Remap
controls and retained-edit rebind remain available on displayed rows.

## Verification boundary

Focused synthetic DOM tests cover empty/one/multiple selection, active ID,
Shift deselection, explicit paging, owned Text bake, generated Text to Path →
Remap, independent batch checks, numeric typing/scrub/undo, Alt handles and unit
moves. Existing core tests cover point-group commands, remapping and text bake.
The numeric-scrub DOM test's old hard-coded import rewrite did not resolve newer
relative imports; it now uses the same generic relative-import rewrite as the
other focused DOM tests.

This does not establish real-browser layout, Windows/EXE performance, or a
complete Inspector acceptance pass. No GUI/EXE test was run. Common multi-point
delta controls and virtualized rows remain separate work; this change bounds DOM
creation with paging instead.
