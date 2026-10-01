# R10 Template-aware Margin/Grid Inspector repair

Status: QUALIFIED bounded cloud candidate / remote synchronization pending. Base `6011a9e5b7d2a9a07670689598f3c41a2f0949ed`.
Owner: dot, sole cloud writer. Existing Confirmed/Should REQ-205, G5 and Route R03/R10.

## Observed gap and bounded contract

Ordinary Artboard Inspector seeds inherited-only Margin/Grid from empty authored
layout. Its family Apply/Clear buttons issue whole-layout `SetArtboardLayout`, which
correctly but inappropriately marks both Template families overridden. Thus editing
Margin can suppress inherited Grid. Inherited-only Grid receives a fresh random ID,
violating the assignment's stable target Grid identity. Grid-to-margin copy refuses
an inherited-only Margin. Repair the UI caller, not the existing whole-layout API.

For assigned targets, seed inherited-only family controls from effective evaluated
values and keep `assignment.grid_id`. Route Margin and Grid Apply/Clear/copy to the
existing corresponding `SetArtboardTemplateOverride` family only. Explicit Clear is
null suppression, not Reset. Reset remains the existing Template action. Preserve
other family inheritance, content, Guides and independent frame sources. Ordinary
unassigned Artboards retain existing commands. Authored local driven-field safety
is unchanged; inherited evaluated values become literal overrides without copying
source-side drivers/expressions. Copy Margin box remains a one-shot Grid edit.

## Acceptance

Source Margin(10,20,30,40), Grid bounds(40,30,100,80), target inherits both.
- Controls show effective numbers. Margin left15 overrides only Margin; inherited
  Grid follows a later source edit. Grid Apply retains target Grid ID and Margin inheritance.
- Clear one suppresses only that family; Reset recovers current source. Counts,
  gutters, all other fields and source document input stay intact.
- Copy inherited Margin box to Grid uses evaluated target dimensions/insets; later
  source Margin edits keep Margin live but do not reflow the copied Grid.
- Preserve existing local driven Margin/Grid sources or reject atomically under the
  current core rules. Never install inherited source metadata on a new local override.
- Public controls, preview versus committed bytes, Undo/Redo, Cancel, stale Session/
  revision, native Host Save/cold process validation and unchanged baseline failures.
No native0.77/core schema or background intent change; offscreen is not visual/Windows
or formal MCP acceptance, and this does not close whole REQ-205.

## Review-driven adjacent transaction repairs

Tests additionally reproduced that the existing Margin/Grid `Cancel draft` buttons
rebuilt controls while leaving the Session preview active. All22 explicit layout
draft/expression Cancel callbacks now cancel that preview before rebuilding the
current Inspector/utility surface. This is cancellation correctness, not a new model.

Read-only review identified Clear reading a stale frame before reaching its guard.
Both Clear callbacks now guard first. Replacement-document tests then exposed stale
recovery rebuilding only Inspector against old Canvas IDs; queued recovery now
refreshes the current document/Canvas first, then its utility surface if open.
Controls are never synchronously destroyed from their callback. Existing whole-layout
replacement and source-preservation core contracts remain unchanged.

## Final cloud receipt — 2026-10-01

- Test-first public-control run reproduced inherited Margin showing0 instead of10.
  Expanded checks exposed actual Cancel and replacement-document stale-recovery bugs;
  these were repaired without relaxing assertions or changing core semantics.
- Dedicated public Window/Host128 checks PASS; focused9/9 Guide/Grid/Canvas/layout
  contracts PASS. Full all-target desktop Release build and real-process smoke PASS.
- Full serial68 CTest:41 pass,1 explicit WIC skip,26 fail. Failed-name set unchanged
  from the67-test scoped-Guide checkpoint. Known platform/fixture/golden obligations
  remain open; this is not full aggregate/Windows/visual/MCP acceptance.
- Tests cover both family flags, Clear/Reset and Clear-to-Apply, native Save/cold
  validation, exact Undo/Redo, source-follow versus one-shot copy, inherited source
  metadata isolation, local driven-source preservation/refusal, plain Artboards,
  valid/invalid preview cancellation and stale Session/revision/replaced-document UI.
- Independent source review found the stale Clear lookup issue; guard and queued
  full-refresh repairs were reviewed and retested with no remaining blocker reported.
- Evidence: ignored `build/d0-evidence/template-layout-{all-build,all-tests,focused,final,smoke}.log`
  and `template-layout-candidate.sha256`. Native0.77 unchanged.
- Next qualification: Template Frame Width/Height literal controls versus retained
  authored fallback and existing axis overrides. No new background semantics.
