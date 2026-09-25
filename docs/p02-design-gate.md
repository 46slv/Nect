# P02 / P02-D — accepted design gate

**Packet:** `P02-DG-01`, revision 3, `DESIGN_CLOSED / IMPLEMENTATION_NOT_STARTED` (2026-09-25). Mission: `Nect/practical-alpha`; one semantic checkpoint per Task. The frozen engineering contract and acceptance cases are in [P02 implementation entry packet](p02-implementation-entry.md), `P02-ENTRY-01` revision 2. This design closure does not claim that any P02 behavior has been implemented.

**Authority and baseline:** [DEC-70, Accepted](https://app.notion.com/p/3e6fd279a6f3811c9448f8bfb8543bde), [REQ-95, Confirmed / Should](https://app.notion.com/p/3e0fd279a6f381ddb9b0c2bbfe8da998), and the [Snapping priority question, Closed](https://app.notion.com/p/3e0fd279a6f381f69ee4fd21033d7f94) were fresh-read on 2026-09-25. Exact pre-closure implementation baseline: `codex/practical-alpha@a60d519a6a63ec339944887693fb3aadde2596a1`, clean and equal to the remote before this documentation change. [P01 qualification](p01-qualification.md) is `CLOSED / QUALIFIED_WITH_ENV_BLOCKERS`: R2/R3/R4/R6/R7 PASS; R1/R5/R8 BLOCKED_ENV. Those three GUI capability routes remain separately qualified and do not block P02 as product defects.

## Accepted owner decisions

| Area | Fixed decision |
| --- | --- |
| Margin / Grid | Margin and Grid bounds are independent authored state. `Set Grid to margin box` copies the current rectangle once. No preset is required in the first slice. An Artboard resize that violates a layout invariant is atomically rejected; no automatic scale or clamp. A future compound edit may be separately specified. |
| Snap | Choose the candidate with the smallest absolute correction in logical screen pixels. On an exact equal-distance tie, use Guide > Grid > Artboard > Text baseline > object edge > object center > equal-gap; then stable target ID and stable source-feature order. Initial threshold is 6 logical px. Overlay visibility and Snap enabled state are independent. First-slice baseline is horizontal, axis-aligned first-line only; residual REQ-33 coverage stays open. Momentary modifier override is not required in the first slice. |
| Align / Distribute | REQ-95 is adopted. Selection distribution holds outer objects fixed and equalizes gaps. Artboard/Grid bounds are virtual outer edges with `n+1` equal gaps. Guide distribution rejects explicitly. Key-object distribution holds the key fixed and requires an explicit nonnegative spacing; objects on either side are placed outward in their original order. The prior proposed implicit average-gap inference is rejected. |
| Shortcuts | Preserve current Nect defaults. Add `G` as a Draw Path alias while retaining `P`. Show collisions and differences in contextual help; do not silently remap existing keys for AE parity. |
| P02-D first slice | Canvas-adjacent Utility Strip; Repeater angle knob plus numeric editor addressing the same existing property Ref; `G` alias plus shortcut collision help. Pointer controls retain fluid/proximity hover, visible keyboard focus and essential state, and stable hit targets. Workspace persistence, property pin/search expansion, and Node drawer are outside this slice. |

## Requirement scope and residuals

| Live Notion leaf | P02 obligation and boundary |
| --- | --- |
| [REQ-32](https://app.notion.com/p/3e0fd279a6f3815589a4f7b41063a271) Confirmed / Must | Define/display Grid and Guides; object and point gesture snap; visible feedback. |
| [REQ-33](https://app.notion.com/p/3e0fd279a6f381bcbfaccc7597c4da69) Confirmed / Must | Edge, center, text-baseline and vertical/horizontal equal-gap candidates. Horizontal axis-aligned first-line baseline is a bounded first slice; rotated/vertical and subsequent-line residuals are not closed. |
| [REQ-157](https://app.notion.com/p/3e0fd279a6f381edb617e9132895f567), [REQ-158](https://app.notion.com/p/3e0fd279a6f3811c8e0af8226c7b95e5), [REQ-95](https://app.notion.com/p/3e0fd279a6f381ddb9b0c2bbfe8da998) Confirmed / Should | Explicit alignment and distribution reference; Grid bounds may be used but never clip or constrain placement. Baseline limits and Guide distribution rejection remain explicit. |
| [REQ-163](https://app.notion.com/p/3e0fd279a6f3819395ecf8bcaaa318c8) Confirmed / Should | On-demand GUI definition, preview and revision of Margin/Grid bounds, counts and gutters without moving existing objects. Overlay can remain visible after setup closes. Preset reuse is deferred. |
| [REQ-9](https://app.notion.com/p/3e0fd279a6f38106a3e5cc2d4d8437b2), [REQ-35](https://app.notion.com/p/3e0fd279a6f38139a468f49654968e2f), [REQ-45](https://app.notion.com/p/3e0fd279a6f381f3adf3e90cbd811a31), [REQ-162](https://app.notion.com/p/3e0fd279a6f3810ab72acc0d73a6c2ac), [REQ-164](https://app.notion.com/p/3e0fd279a6f381fcab0cd396763100fc), [REQ-179](https://app.notion.com/p/3e0fd279a6f38190821bf2926a9ee650) Confirmed | Low-noise UI, shortcut intent, readable hover/focus/state, shared Repeater angle Ref, and compact strip with stable icon controls. |
| [REQ-94](https://app.notion.com/p/3e0fd279a6f381d58ac7f036159557d8), REQ-39/40/43/44/165/166 | Candidate scope remains separate. Measurement/annotation, workspace persistence, property pin/search and Node drawer are not first-slice obligations. |

## Engineering closure and next gate

`P02-ENTRY-01` revision 2 fixes native 0.14 shape and 0.1–0.13 migration, stable identity and coordinates, validation, command/error/stale/Undo/recovery/persistence behavior, exact snap and alignment results, allowed writes, positive and negative fixtures, and actual GUI/runtime acceptance. The packet's numerical limits and new error names are engineering contract choices for the next Task, not additional Notion product requirements. The next implementation Task must fresh-check project, model, effective authority, exact HEAD and single-writer ownership, then record `TAKEOVER_ACK` before code changes. The intended profile is Sol-high Coordinator/Reviewer with a fresh disposable Luna-Max Worker; this document does not infer authority from a prior top-level Task.

**Stop boundary:** P02 product code, native schema and UI were not changed in this Task. G05/G11 and the full REQ-33/95 acceptance are not claimed complete. No main merge, release or distribution follows from this design closure.
