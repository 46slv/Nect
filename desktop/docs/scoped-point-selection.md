# Scoped point selection

Base: verified PR44 `5e0b69adae9a6798df3e78fbee93a2c474b48929`, branch
`codex/color-operators-v1`. PR44's candidate/receipt were verified before a new
build in `C:/Users/shiro/Documents/Codex/2026-10-21/task-13/color-operators-artifact-c9ea676`,
SHA256 `d8d325786f0d20b6630261125271d495b480f021cdf036c5a2b4b17682fbcd22`.
Preserve all PR40–44 sealed candidates.

Source: adopted OUTLINE-EDIT-UI in [Nect Home](https://app.notion.com/p/3dffd279a6f381cca8c7c4dec111b131)
and research08 section4 in [research](https://app.notion.com/p/3fcfd279a6f381e78204fa7e1053818b).
Current point panels have view-only unit/contour selectors, 24-row paging,
individual stable-ID selection and distinct whole-unit actions; no scoped
select-all/invert equivalent exists.

Add explicit Select all / Invert buttons inside the existing point panel, with
the current path/unit/contour name and full target anchor count. Target membership
comes from all current source anchors in that scope, independent of paging and
selected-only display. Select all replaces selection with that set; invert
replaces it with that set's complement relative to current selection. Outside
points are dropped, not added or retained implicitly. An empty set disables the
actions. Active point/row follows a retained selected ID or the first new ID;
empty result clears focus. Existing Canvas selection remains the sole owner.

Source contract decision: locked/driven anchors are selectable already; including
them must not silently shrink the set. Keep them selected and retain existing
whole-selection edit rejection/disabled delta controls. Generated Remap uses its
actual current input contours; its independent checkbox batch state is unchanged.
No bake/rebind or held edit changes. Capture scope IDs, all target IDs, source
fingerprint, layer/session/revision in transient action tokens; stale targets or
ongoing edits reject before any selection change. Browsing may normalize an old
scope for display, but an old action token never falls back to another scope.

No authored geometry, schema, dirty/history, Color, dither, shortcut or panel
layout changes. Focus checks on empty/single/compound/hole/generated/outlined
sources, page-spanning and selected-only inversion, both panel copies, stable-ID
Canvas/active-row synchronization, locked/driven and stale rejection, and
document/file/history neutrality. Use scoped native renderer checks; label
unavailable physical input/IME, OS file dialogs and cold reopen separately.
