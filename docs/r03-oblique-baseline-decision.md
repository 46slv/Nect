# R03 oblique Text baseline Snap — interaction decision gate

**Status:** DEFERRED by the user's 2026-09-26 interaction choice: preserve axis-only baseline Snap for now. **Source:** `codex/practical-alpha@3eeb3656e49342e2ec6423ac2fea35ed4d35bb57`, clean and remote matched at analysis. **Authority:** the current user's explicit choice, DEC-71, Confirmed Must REQ-33, R03 Route, P02-C1 and R03-BASELINE-01/02/03/04. This note does not approve oblique implementation or change existing Snap behavior. REQ-33 remains partially open.

## Established geometry

A measured local horizontal line has anchor `(0, baseline_y)` and tangent `(1, 0)`; a measured vertical-writing column has anchor `(baseline_x, 0)` and tangent `(0, 1)`. Compose the Text path and object/parent world transforms. For a nonsingular resulting tangent `t`, use a unit normal `n` and line offset `d=n·anchor`. Parallel source/target lines have the same unoriented tangent, so orient their normals consistently. Given a raw object translation `r`, their unique minimum-length alignment correction is `c=(d_target-d_source-n·r)n`. This remains translation-only and leaves native Text metrics derived. Nonparallel lines cannot coincide under translation alone and are ineligible. The existing 6-logical-pixel threshold could apply to `|c|*frozen_zoom` without DPR dependence.

The geometry does **not** decide how `c` competes with the current independent X and Y winners. P02-C1 explicitly picks the shortest correction on each axis, then priority and stable ID on exact ties, and permits both axes to win. It defines no vector distance or rule for replacing one or both axis winners. The current Canvas data model and feedback carry X/Y candidates separately; an oblique candidate would need vector matching and a visible line guide/normal correction.

## Concrete conflict

At zoom 1, an oblique baseline whose unit normal is `(0.6, 0.8)` needs correction `(3, 4)` du, length 5 logical px. An eligible Guide needs `+1` du on X; an eligible Grid needs `+1` du on Y. Current P02-C1 selects both axis candidates and produces `(1, 1)` with Guide/Grid feedback. Selecting the oblique baseline instead would produce `(3, 4)` with Text feedback. Combining it with just one axis candidate generally breaks baseline coincidence. Both outcomes honor some existing priorities but yield different authored translations and interaction feedback; REQ-33's Acceptance does not choose one.

## Options considered and current decision

1. **Preserve current axis behavior (chosen):** Keep arbitrary-angle baseline Snap deferred; existing measured horizontal, vertical and quarter-turn lines continue to work. REQ-33 remains partially open.
2. **Add vector arbitration:** Allow parallel oblique baselines, then define whether the complete 2D oblique correction competes against the pair of independent axis winners by Euclidean correction length, whether Guide/Grid priority applies only on exact vector-distance ties, and what occurs when only one axis has a winner. This gives oblique baselines a chance to win but can change a familiar Guide/Grid drag outcome.
3. **Axis candidates always win:** Offer an oblique candidate only when neither axis has an eligible candidate. This preserves existing Snap outcomes but lets a farther axis candidate suppress a closer oblique line, changing the meaning of distance-first priority.

If oblique behavior is reopened later, first freeze exact positive/negative fixtures for parallel horizontal and vertical-writing oblique lines, measured line/column selection, 5/6/7 logical-pixel corrections at zoom and DPR variants, nonparallel/degenerate/ambiguous lines, concurrent axis candidates, stable tie order, transient feedback, one `TranslateObjects` release/Undo, native round trip and unchanged numeric/API edits. A new product interaction choice would be needed before implementation.
