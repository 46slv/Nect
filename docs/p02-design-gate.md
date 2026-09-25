# P02 / P02-D — design gate packet

**Packet:** `P02-DG-01`, revision 1, `DESIGN_GATE` (no implementation authority).

**Mission:** `Nect/practical-alpha`; one semantic checkpoint per Task.

**Source:** [Notion Worker packets revision 3, section 5 and U01/U10](https://app.notion.com/p/3e3fd279a6f381ee8ab0ea5ea614c006) (`WORKING DRAFT`, fetched 2026-09-25); [full route and REQ mapping](https://app.notion.com/p/3e3fd279a6f381b4ba9dd2d1bf066e0f); [P01 qualification](p01-qualification.md). Notion owns the accepted product requirement; this file is a reviewable implementation-contract proposal, not a replacement for the requirement leaves.

## Entry and current baseline

- P01 has classified all R1–R8: R2/R3/R4/R6/R7 `PASS`; R1/R5/R8 `BLOCKED_ENV`. No accepted product or manual defect and no repair candidate. The three blocked GUI routes are described in `p01-qualification.md`; do not silently count those recipes as fully accepted.
- Preparation baseline: `D:\Documents\Nect`, `codex/practical-alpha@22cdee057e23417936b2e810426fbf2ebc3cd768`, equal to `origin/codex/practical-alpha` before this packet. The final handoff must fresh-read Git HEAD and remote equality. The current native format is 0.13; the P01 Release executable SHA-256 is `36DBD80439951364E4A7857586C7DB20705394C0A950087FA37DF0C325047739` (no exact compile-SHA manifest).
- Read first: `AGENTS.md`, `CURRENT_GOAL.md`, `ARCHITECTURE.md` (owners, coordinates, Session, view state, gestures), `README.md` (current Snap and Align), `docs/model-v0.md` plus the relevant native schema for any saved-field proposal, and `docs/quality.md` for the acceptance boundary. Fresh source and runtime readback outrank this preparation snapshot.
- Current Snap is a per-window view preference for object-body drag to active Artboard/visible-object geometric edges and centers within 6 logical screen pixels. Point/handle and exact numeric/API edits do not snap. Existing whole-object align uses selection envelope or Artboard; equal-gap distribution uses geometric bounds with fixed outer objects. These subsets do not close persistent Grid/Guide/Margin, point-to-guide, baseline, dynamic equal-gap snap or all reference choices.

## Required product coverage

| Source family | Requirement IDs and obligation to carry into design | Present boundary |
| --- | --- | --- |
| G11 / U01 | `REQ-32`: objects **and points** snap to Grid/Guide. `REQ-33`: baseline and equal-gap candidates in addition to edge/center. `REQ-157/158`: explicit selection, key object, Artboard and Grid alignment references. | Current Snap and align are subsets. `REQ-94/95/163` are in the G11 route, but their exact leaf acceptance must be read before claiming closure. |
| G05 / P02-D / U10 | `REQ-9/35/39/40/43/44/45/162/164/165/166/179` appear in the full-route UI/shortcut family. Make an action-by-action mapping to their exact leaves before accepting any adapter. | Existing UI presence or an AE-like appearance alone does not close these requirements. |

The next design Task should retrieve the exact leaves for `REQ-94/95/163` and the P02-D IDs relevant to each chosen adapter. It should mark any remaining product decision in Notion's owner, and leave unapproved candidates as candidates.

## Proposed P02 contract to decide

| Boundary | Candidate contract and review question |
| --- | --- |
| Authored definitions | Stable-ID Guide with axis and position in a Composition coordinate plane; Artboard-bound margin/grid with explicit bounds. Decide field types, ID lifetime, units, validation, duplicate/delete behavior, native version/migration and unknown-version handling before code. Definition edits must be atomic Session commands with Undo and native/recovery readback. Guide and grid state must not be duplicated in Desktop view preferences. |
| View and gesture | Guide/grid visibility and snap enablement are separate view choices. Hidden presentation does not delete an authored definition. Overlay and hover are Desktop-owned, non-exportable. Screen-pixel threshold stays defined at zoom/DPI boundaries. Decide candidate priority/ties, multiple simultaneous candidates, hidden/locked targets, group scope, and cancel behavior. |
| Placement and exact edits | Grid is a suggestion, not a placement constraint; objects may be outside it. Changing a definition does not reflow existing objects. Snapping applies to eligible gestures, not exact numeric/API values. A point drag converts its world candidate through the object's invertible world-to-local transform and leaves other points unchanged. If the selected axis is driven or the transform cannot be inverted, reject the entire proposed edit with no revision/partial mutation. |
| Align and distribution | Extend the existing Session command family only where a reviewed reference/candidate needs it. Selection, key object, Artboard and Grid targets require explicit meaning; baseline source and equal-gap calculation require separate candidates. One accepted action is one Undo. Preserve stable object/point IDs and the source geometry. |

Likely later implementation slices are P02-A native definitions/commands/migration, P02-B overlay/settings UI, and P02-C object/point snap plus explicit align. All slices must use the **same Document and Session authority**. This is a dependency map for design review, not a request to begin those slices.

### Fixed oracles for contract review

| Case | Input and expected result |
| --- | --- |
| Grid geometry | Artboard 960×640, left/right margins 40, two columns, gutter 20: inner width 880, column width 430, intervals `[40,470]` and `[490,920]`. A vertical Guide at x=100 moved to x=120 leaves existing object coordinates unchanged. Save/reopen retains the definitions and stable IDs. |
| Screen-space snap | A drag candidate 5 logical screen pixels from the Guide is within the current 6px threshold; 7px is outside. At 50%, 100% and 200% zoom, the same screen distance yields the same choice. Resolve equal-distance priority by an explicit rule before acceptance. |
| Point and exact edits | Point snap moves only the selected point after world-to-local conversion. Exact numeric/API values stay exact. A driven-axis move, noninvertible parent, wrong coordinate plane, or mixed Group-and-child selection rejects atomically when its defined contract cannot represent the edit. |
| Equal gaps | In one Composition with identity transforms, A=`[0,10]`, B=`[30,50]`, C=`[90,100]`. Equal horizontal gaps keeps A/C fixed and puts B at `[40,60]`, for gaps of 30; center-spacing is not an equivalent oracle. |
| Key object | With B as key object, left-align A/B/C to B's left edge 30 and leave B unmoved. The UI must reveal which reference was selected. |
| Persistence/interaction | Native save and fresh reopen, changed definition, one Undo, canceled gesture, read-only GUI/API parity and SVG/PNG without overlay. The actual viewport performance gate uses a recorded machine/scene/DPI and p95 frame interval ≤33.3ms for the specified 30fps path, not headless timing or average FPS. |

## P02-D daily UI gate

Start with an inventory: current menu/action/shortcut → corresponding AE concept → keep/add candidate → exact REQ leaf → actual UI and Session command. Preserve distinct meanings when shortcuts collide; do not modify the user's global shortcut settings. Keep the standard workspace free of timeline/transport controls.

Review binary icons in ON, OFF, hover, focus and disabled states. The name must be readable on hover/focus, state cannot rely on color alone, and proximity feedback must not move hit targets. Check keyboard focus, caption, scrolling/reachability, narrow width and DPI on representative screens.

Angle knob plus numeric field, Canvas utility, and property search/pin should address the **same existing Session property**. One knob drag is one Undo; Escape cancels; the field, API and Canvas read back the same value. Do not add a display-only Scalar as a second authority. Panel position/width/collapse belongs to workspace state, separate from native artwork; restart restores the workspace while native/SVG remains unchanged. A proposed panel arrangement stays a prototype until its ownership and persistence contract are approved. Command correctness is machine-verifiable; comfort and visual clarity need actual user/GUI acceptance.

## Decisions and exit gate

1. Fetch exact requirement leaves, especially `REQ-94/95/163` and each selected P02-D adapter; record the accepted obligation and unresolved product choice with its Notion owner.
2. Decide Guide/Grid/Margin schema, coordinate/bounds rules, version migration, stable IDs and atomic command/error/Undo behavior against `ARCHITECTURE.md` and the current native schema.
3. Decide snap candidate types and priority/tie policy, baseline source, hidden/locked and Group scope, point conversion, key-object selection UI and failure behavior using the positive/negative oracles above.
4. Decide the smallest P02-D adapter set and map each to exact REQs, one live Session property/command, workspace ownership and actual UI acceptance.
5. Publish a reviewed implementation packet with frozen fixtures, tolerances, owner/files, allowed writes and focused tests/runtime checks. Only then may a separately authorized implementation Task start. A design disagreement returns to the requirement/contract owner; a future implementation mismatch returns to its bounded implementation packet. Do not weaken an oracle to match output.

**Stop boundary:** This Task may refine the design contract and evidence. It must not change product code/native schema, start P02-A/B/C or P02-D implementation, or claim G05/G11 complete merely from the current UI and tests. The next Task first verifies repository root, branch/HEAD, clean worktree, effective Full access/approval mode, and single-writer ownership before any local write.
