# R03/R10 inherited Template Grid Align/Distribute

Status: CORE/JSON + QT/HOST VERTICAL QUALIFIED / AGGREGATE PLATFORM_LIMITED. Base a799d91b6119429d19e77f5a25d54220b185aed8.
Owner: dot, single cloud writer. Existing Confirmed REQ-157/205 and G5 apply.

The current Grid solver already evaluates reference bounds but discovers only
authored Grid records. Reuse effective_grid_artboard for local and foreign-plane
discovery, and enumerate evaluated Artboards in the existing Window selector.
Keep grid:ID, target-local Grid identity, all solvers, errors and native0.77.
No new command, reference syntax, persistent constraint or product expansion.

Fixed fixture: source origin(-500,-600), local Grid(40,30,100,80). A origin(1000,200),
B(2000,400) inherit without authored layouts. A bounds[1040,230,1140,310], B bounds
[2040,430,2140,510]. Align min/center/max and distribute independent 10/20/10-sized
rectangles. Expected A x lefts1055/1080/1115; B2055/2080/2115; A y tops240/260/290.
Expected values come from fixture arithmetic, not the production evaluator.

Prove source bounds edits, target/source frame movement separation, stable identity
through rename/reorder, local override/reset, explicit null suppression, source
Grid removal and detach. Missing/suppressed Grid, foreign effective Grid and
insufficient span refuse with exact state/revision/History preservation. Native
cold read, Undo/Redo and public Window selection/actions use the same commands.
Existing authored Grid/global/Guide behavior remains intact. No requirement or
platform-wide acceptance promotion follows from this bounded repair.

## Verification — 2026-10-01 UTC

Baseline dedicated test failed to resolve template-grid-A before the product fix.
Final dedicated core fixture PASS45 checks; Qt Window/Host fixture PASS35 checks.
This includes evaluated source bounds links/expressions, override/reset/null,
detach, foreign inherited Grid rejection, fixed numeric alignment/distribution,
reorder/selector fallback, Undo/Redo, canonical Save and a fresh process that
reopens the native file, performs another inherited-Grid alignment and returns
exact translation2040. The input native file remains unchanged. Independent
read-only review found no blocking source issue; suggested coverage was added.

Focused six Guide/Grid/Template contracts PASS. Full all-target desktop Release
build PASS; full65 CTest38pass/1skip/26fail. Failed-name comparison with the prior
63-test aggregate has no added or removed failures. These retained platform,
fixture and golden-output obligations are not passes. Real-process smoke PASS.
Raw logs: ignored build/d0-evidence/grid-align-*. No Windows/hands-on/MCP/whole
Requirement promotion. No solver, command protocol or native version change.
