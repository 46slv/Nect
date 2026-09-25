# P02 / P02-D — design gate packet

**Packet:** `P02-DG-01`, revision 2, `DESIGN_GATE / OWNER_DECISIONS_PENDING` (no implementation authority).

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

Revision 2 below retrieves the exact `REQ-94/95/163` leaves and the P02-D leaves relevant to its proposed adapters. It marks remaining product decisions for the Notion owner and leaves unapproved Candidates as Candidates.

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

An Angle knob, numeric field, and any property search/pin editor for that angle should address the **same existing Session property**. One knob drag is one Undo; Escape cancels; the field and API read back the same value. Canvas utility view toggles are separate view state. Do not add a display-only Scalar as a second authority. Panel position/width/collapse belongs to workspace state, separate from native artwork; if that Candidate is adopted, restart must restore the workspace while native/SVG remains unchanged. A proposed panel arrangement stays a prototype until its ownership and persistence contract are approved. Command correctness is machine-verifiable; comfort and visual clarity need actual user/GUI acceptance.

## Decisions and exit gate

1. Fetch exact requirement leaves, especially `REQ-94/95/163` and each selected P02-D adapter; record the accepted obligation and unresolved product choice with its Notion owner.
2. Decide Guide/Grid/Margin schema, coordinate/bounds rules, version migration, stable IDs and atomic command/error/Undo behavior against `ARCHITECTURE.md` and the current native schema.
3. Decide snap candidate types and priority/tie policy, baseline source, hidden/locked and Group scope, point conversion, key-object selection UI and failure behavior using the positive/negative oracles above.
4. Decide the smallest P02-D adapter set and map each to exact REQs, one live Session property/command, workspace ownership and actual UI acceptance.
5. Publish a reviewed implementation packet with frozen fixtures, tolerances, owner/files, allowed writes and focused tests/runtime checks. Only then may a separately authorized implementation Task start. A design disagreement returns to the requirement/contract owner; a future implementation mismatch returns to its bounded implementation packet. Do not weaken an oracle to match output.

**Stop boundary:** This Task may refine the design contract and evidence. It must not change product code/native schema, start P02-A/B/C or P02-D implementation, or claim G05/G11 complete merely from the current UI and tests. The next Task first verifies repository root, branch/HEAD, clean worktree, effective Full access/approval mode, and single-writer ownership before any local write.

## Revision 2: exact leaves and decision status (2026-09-25)

This review read the individual Notion Requirements pages, not just the route's ID list. `Confirmed` means product intent is adopted; it does not approve this proposed schema, UI, or implementation packet. The [Requirements home](https://app.notion.com/p/3dffd279a6f381cca8c7c4dec111b131) and individual leaves own product acceptance. No named approving person is recorded on these leaves; the Nect product/requirements owner must accept the choices marked `OWNER` below.

| Leaf | Exact obligation relevant here | Decision status |
| --- | --- | --- |
| [REQ-32](https://app.notion.com/p/3e0fd279a6f3815589a4f7b41063a271) Confirmed/Must | Display/configure Grid and user Guides; snap both objects and points, with visible snap feedback. | In scope; current body-only Snap is a subset. |
| [REQ-33](https://app.notion.com/p/3e0fd279a6f381bcbfaccc7597c4da69) Confirmed/Must | During movement, show and snap to other objects' edges, centers, baselines, and vertical/horizontal equal-gap candidates. | In scope; baseline and dynamic equal-gap need new candidates. |
| [REQ-94](https://app.notion.com/p/3e0fd279a6f381d58ac7f036159557d8) Candidate/Could | Measure distance, angle, and size on Canvas; optionally retain an annotation object. | Separate measurement-tool candidate; no P02 measurement/annotation claim or implementation permission. |
| [REQ-95](https://app.notion.com/p/3e0fd279a6f381ddb9b0c2bbfe8da998) Candidate/Should | Explicit align/distribute by edge, center, baseline with key-object, Artboard, or selection reference. | Existing geometric align and equal-gap are partial; adopting the remaining candidate is `OWNER`. |
| [REQ-157](https://app.notion.com/p/3e0fd279a6f381edb617e9132895f567) Confirmed/Should | Explicit selection/key-object/Artboard or Canvas/Guide or Grid reference for edge, center, distribute, without unrelated command semantics. | In scope; guide and distribution reference semantics are `OWNER`. |
| [REQ-158](https://app.notion.com/p/3e0fd279a6f3811c8e0af8226c7b95e5) Confirmed/Should | Grid left/right/top/bottom/center can be an alignment reference; Grid never clips or constrains placement. | In scope; independent grid bounds are required. |
| [REQ-163](https://app.notion.com/p/3e0fd279a6f3819395ecf8bcaaa318c8) Confirmed/Should | GUI can preview, define, and revise margins/Grid bounds, rows/columns, gutters, divisions and dimensions at project start or later; edits do not move/clip objects; optional presets; setup surface need not remain open and overlay remains visible after closing it. | In scope; exact bounds/resize and preset semantics are `OWNER`. |

P02-D leaf selection is deliberately small. [REQ-9](https://app.notion.com/p/3e0fd279a6f38106a3e5cc2d4d8437b2) Confirmed/Must supplies the low-noise/readability constraint; [REQ-35](https://app.notion.com/p/3e0fd279a6f38139a468f49654968e2f) Confirmed/Should prioritizes AE defaults only for equivalent commands; [REQ-45](https://app.notion.com/p/3e0fd279a6f381f3adf3e90cbd811a31) Confirmed/Should requires stable hit targets, hover names, and essential state visible without hover. [REQ-162](https://app.notion.com/p/3e0fd279a6f3810ab72acc0d73a6c2ac) Confirmed/Should requires one angle property to support radial drag and precise numeric/scrub editing with explicit wrap/clamp semantics. [REQ-164](https://app.notion.com/p/3e0fd279a6f381fcab0cd396763100fc) Confirmed/Should asks for a compact Artboard/Preview utility strip with light controls and expandable detailed setup. [REQ-179](https://app.notion.com/p/3e0fd279a6f38190821bf2926a9ee650) Confirmed/Should requires clickable binary icons whose OFF/ON/hover/focus/disabled states and names are distinguishable.

[REQ-39](https://app.notion.com/p/3e0fd279a6f38169a590c20d4a664bd5), [REQ-40](https://app.notion.com/p/3e0fd279a6f381a6917af024e57d8398), [REQ-43](https://app.notion.com/p/3e0fd279a6f381c9b2f3d39850650b9e), [REQ-44](https://app.notion.com/p/3e0fd279a6f381aab3adf0bc77442be7), [REQ-165](https://app.notion.com/p/3e0fd279a6f381f19b5fc0d79f939603), and [REQ-166](https://app.notion.com/p/3e0fd279a6f38154ade4c6a160fa2e9a) remain Candidates. Existing right Properties dock is not acceptance of REQ-39, a future Node drawer belongs with its graph contract, and workspace persistence/direct overlay edit modes must not be smuggled into Confirmed coverage. View-versus-document labeling can still prevent confusion without claiming Candidate REQ-166 closed.

### Native/Session contract proposed for owner review

Current native 0.13 has strict `additionalProperties: false`: `Composition` has ID/name/roots/Artboards, and an `Artboard` has ID/name/x/y/width/height/optional size parent. `Document` has no Guide/Grid/Margin data. `src/io.cpp` loads versions 0.1–0.13 with explicit key lists and emits 0.13. Therefore any authored definition requires a reviewed native 0.14 schema and migration; a view preference or unknown 0.13 key is not a valid shortcut.

Proposed 0.14 shape, **not an implemented schema**:

```text
Composition.guides[] = {id, axis: x|y, position: finite du96, name}
Artboard.layout? = {
  margin?: {left, top, right, bottom: nonnegative du96},
  grid?: {id, bounds: {x, y, width, height: Artboard-local du96},
          columns: positive integer, rows: positive integer,
          column_gutter, row_gutter: nonnegative du96}
}
```

`x` Guide is a vertical constant-X line; `y` Guide is horizontal. A Guide belongs to one Composition's Y-down `du96` plane and is not an Artboard child. Its opaque ID is document-unique and survives rename/reorder/move/save/recovery/Undo; delete removes that definition, and reinsertion allocates a new ID. One optional layout belongs to an existing stable Artboard ID; Margin and Grid can be defined independently. One optional Grid ID is document-unique so a copied Artboard receives a new Grid ID. Margins have no separate ID or snap line: the Artboard ID addresses them. Grid bounds are **explicit**, independent of margin insets; a GUI “Set Grid to margin box” action copies the current computed rectangle once, without a continuing hidden link. Moving the Artboard translates its Grid in Composition space; changing its size leaves local Grid numbers unchanged and must validate them against the new size. A failing size edit rejects atomically. This independence and resize rule need `OWNER` acceptance.

Require finite values; when present, positive Grid bounds within the evaluated Artboard, nonnegative margins with left+right < width and top+bottom < height, counts 1–1000, and positive computed cell dimensions: `(bounds.width - (columns-1)*column_gutter)/columns > 0`, likewise rows. Do not silently clamp, rescale or reorder user values. The chosen limits are engineering proposals, not accepted requirement thresholds. A missing layout means no authored Grid/Margins; an older 0.1–0.13 file migrates to empty guides and absent layout with all existing IDs/geometry unchanged. New writes carry 0.14; older clients must fail explicitly on unknown version and leave source bytes intact. Load → migrate → add/edit/delete → save → fresh reopen → Undo/recovery is required. Native, recovery and read-only API inspect must agree; SVG/PNG omit overlay definitions while retaining artwork. No separate Desktop copy of authored definitions.

Core commands proposed: `add_guide`, `update_guide`, `delete_guide`, and `set_artboard_layout` (including clearing it), with exact Composition/Artboard/Guide IDs and expected Session revision. Duplicate Guide IDs, unknown IDs, wrong plane, invalid geometry and stale revisions reject the whole batch with no new revision/history. A gesture previews on the Session starting snapshot; Escape cancels and one accepted drag makes one Undo. `Document` equality/history retained-size accounting, validation, `nect_io` encode/decode/API command conversion, and desktop readback must all include these new fields. Artboard duplication/deletion follows the same atomic ownership rule.

### Gesture/snap and alignment contract proposed for review

- Freeze the moving selection, target identities/geometry, active Composition, entered Group scope and zoom at gesture start. Express the threshold in **logical screen pixels** (`6 / zoom` world du for axis-aligned view); device-pixel ratio affects physical rasterization, not this distance. Exact field/API/keyboard nudges remain exact. Reject stale Session revisions rather than rebasing a drag silently.
- Compute eligible X and Y candidates independently. Choose minimum absolute logical-screen correction. On exact equal distance, proposed type order is Guide → Grid line/bound → Artboard edge/center → Text baseline → object edge → object center → equal gap; then stable target ID, then source-feature order. Simultaneous X and Y winners can both apply; feedback names each source. This order is deterministic but `OWNER` must approve the user-facing priority. A hidden Guide/Grid overlay and their snap toggles are independent view settings; if snap stays ON while overlay is hidden, transient snap feedback still identifies the target. Hidden objects are excluded. Nect has no authored lock model today, so “locked target” behavior is unresolved, not simulated.
- Derive Grid lines from the explicit rectangle, counts and gutters. For a two-sided horizontal equal-gap candidate, fixed neighbors `[L0,L1]` and `[R0,R1]` with moving width `w` give target left `(L1 + R0 - w)/2`; offer only when the gap is nonnegative and all objects are in one Composition. A candidate formed from one repeated neighboring gap requires a separate exact fixture before implementation. Keep current exact `DistributeObjects` meaning: sort by starting minimum, preserve both outer objects, equalize **gaps**, reject initial overlaps. Do not replace it with center spacing.
- Baselines must come from the text layout's actual line metric, transformed into Composition space; glyph bounding-box bottom is not a baseline. Current `TextLayout` exposes bounds/outlines but no baseline value. Proposed first slice uses a horizontal, axis-aligned first-line baseline, with explicit no-candidate behavior for rotated/vertical/multi-line cases until the `OWNER` approves their scope. Never fabricate a baseline from bounds.
- Object-body snap proposes a world displacement through the existing shared `TranslateObjects` solver, including one selected object; do not use a one-object local `transform.tx/ty` shortcut for the same world request. A point snap maps its world candidate through that object's initial invertible world inverse into `Set` commands for the selected stable point IDs only. Generated points use existing Point Edit ownership. Required driven axes, singular inverse, wrong Composition, and mixed structural Group/child selection reject the complete preview/commit; no neighboring point, object, or source geometry changes.
- Normalize `align_objects` to one explicit reference variant: selection envelope, stable key-object ID, Artboard ID, Grid ID, or Guide ID. Retain the old `artboard` JSON field as a documented compatibility alias, accepting exactly one form and converting both into the same Session command. A key object must belong to the selection, be visibly named in the UI, and remain fixed while other selected objects align to its initial geometric bound. A Guide supports only its matching axis; Grid references its explicit bounds. Existing selection/Artboard geometric alignment and fixed-outer equal-gap distribution keep their current behavior. Whether Guide is an accepted target and how key-object/Artboard/Grid references govern **distribution** (rather than alignment) is `OWNER`; no implementation packet may infer it from the current two-target chooser.

### P02-D focused action inventory and minimum adapters

The inventory below is from `src/desktop/window.cpp` and Canvas, checked against [Adobe's current After Effects Windows shortcut reference](https://helpx.adobe.com/after-effects/desktop/get-started/keyboard-shortcuts/keyboard-shortcuts-reference.html). An AE concept is a comparison, not permission to change Nect's current key. The user's global shortcut settings remain untouched.

| Current Nect action/key | AE concept and collision | P02-D choice / leaf |
| --- | --- | --- |
| File New/Open/Save/Save As (`Ctrl+N/O/S/Shift+S`) | AE project/composition/file actions differ in object meaning. | Keep Nect document meanings; REQ-35 review only. |
| Edit Undo/Redo/Delete (`Ctrl+Z`, `Ctrl+Shift+Z`, `Delete`); Duplicate (`Ctrl+D`) | Undo/Redo/Duplicate have direct editing analogues. | Keep; REQ-35. |
| Edit Group/Ungroup (`Ctrl+G`, `Ctrl+Shift+G`); Arrange (`Ctrl+[`, `Ctrl+]`, plus Shift) | AE precompose/layer stacking is not Nect Group/paint-order semantics; AE layer order uses Ctrl+Alt+arrows. | Keep Nect meanings, document non-equivalence; REQ-35. |
| Edit Rotate/scale selection (`Ctrl+Shift+T`) | AE uses `Ctrl+Shift+T` for Effect Controls. | Shortcut collision: `OWNER` chooses Nect default; do not silently rebind. |
| Edit Anchor (`Y`); Center Anchor (menu only) | AE `Y` is Pan Behind; center anchor is `Ctrl+Alt+Home`. Nect's authored Anchor preserves visual placement through its matrix model. | Keep current `Y`; candidate alias for Center Anchor only after conflict check; REQ-35. |
| Add Draw Path (`P`); Add Curve (`Ctrl+Shift+P`) | AE `G` is Pen; `P` shows Position and `Ctrl+Shift+P` opens Position dialog. | Material collisions: `OWNER` chooses aliases/defaults; existing creation route stays functional. |
| View Fit Artboard (`Ctrl+0`), Fit selection (`Ctrl+2`), Fit all (`Ctrl+Shift+0`), Snap (no key), History (`Ctrl+Shift+H`) | AE Fit is `Shift+/`; Nect Artboards are output rectangles rather than time Compositions. | Keep explicit Nect scope; no blind AE remap. Snap toggle candidate for utility strip; REQ-35/164/179. |
| Canvas Space-drag, wheel zoom, arrows/Shift+arrows | AE temporary Hand uses Space; its arrow movement is pixel-at-magnification while Nect uses 1/10 world du. | Keep the shared gesture, label differing nudge units; REQ-35. |
| Edit Align/Equal gaps (menu/Inspector, no key) | AE Align/Distribute concept. | Reuse Session commands and explicit reference display; REQ-157, REQ-95 Candidate. |

Only three UI adapters are proposed for the first separately authorized implementation packet: **(1)** a compact Artboard utility strip for view-local Guide/Grid/Snap toggles, named ON/OFF/focus/disabled icons, and a document-edit setup popover that calls Session; **(2)** a radial control and numeric row for the existing Repeater `op.<id>.rotation` Scalar, using one gesture/Undo and the same property Ref that API/Inspector read; **(3)** contextual shortcut help that identifies the above collisions without changing key assignments. The Repeater angle is an initial concrete angle property, not a display-only Scalar or a claim that every angle property is covered. Property search/pin and a Node drawer are outside this minimum set. Utility toggles are view state and **do not** address the angle Scalar; the former revision's blanket “same property” phrasing applies only to multiple editors of one authored property.

Do not claim workspace restoration under Candidate REQ-43 until its owner adopts a persistence contract. If adopted later, `QDockWidget` layout/width/collapse belongs in separate workspace preferences, never native artwork; test restart and document switching while native/SVG remain unchanged. Real GUI acceptance must inspect hover and keyboard focus names, all five binary icon states, stable hit targets, narrow width/DPI reachability, scroll/caption access, and whether setup closes without hiding the resulting overlay. Command correctness and visual comfort have separate evidence.

### Independent acceptance fixtures and future packet boundary

| Fixture | Independent expected result / negative boundary |
| --- | --- |
| `DG-GRID-01` | Artboard `(0,0,960,640)`, margins L/R=40, T/B=20; Grid bounds local `(40,20,880,600)`, 2 columns, gutter 20, one row: column X intervals `[40,470]`, `[490,920]`. Moving Artboard X to 200 moves Grid intervals to `[240,670]`, `[690,1120]`, with object X unchanged. Moving Guide x=100→120 also leaves objects unchanged. |
| `DG-SNAP-02` | Raw candidates 5/7 logical px from a Guide: at zoom 0.5 world offsets 10/14 du; zoom 1 offsets 5/7; zoom 2 offsets 2.5/3.5. Only the 5px case snaps with 6px threshold. Same-coordinate Guide/Grid tie chooses Guide under the proposed priority; test two DPRs. |
| `DG-POINT-03` | Parent translation `(100,50)`, object local scale 2, selected point local `(10,20)` → world `(120,90)`. X Guide 125 yields local `(12.5,20)` on eligible snap; adjacent point is byte-for-structure unchanged. Parent scale X=0 or the selected point's X Scalar being driven rejects with unchanged document/revision/history. |
| `DG-GAP-04` | Identity A=`[0,10]`, B=`[30,50]`, C=`[90,100]`: exact equal-gap command fixes A/C and moves B to `[40,60]`, producing 30/30 gaps. Dynamic middle-object candidate proposes the same target. Initial overlap rejects, not center spacing. |
| `DG-ALIGN-05` | Identity A=`[0,10]`, B=`[30,50]`, C=`[90,100]`: with B explicitly chosen as key, left-align all three to X=30 and keep B unchanged. Grid left X=40 moves a selected `[0,10]` object to `[40,50]`; a different Composition or mixed Group/child selection rejects atomically. |
| `DG-IO-06` | Load a saved 0.13 fixture, add/edit definitions, save 0.14, fresh reopen and inspect same stable IDs/values. One Undo restores the prior definition; canceled drag leaves revision/history unchanged; recovery matches the exact committed revision. Unknown future version rejects without file mutation. SVG/PNG lack overlay elements. |
| `DG-UI-07` | Inspector numeric and Repeater knob/API read the same Ref. One knob drag is one Undo; Escape cancels. Utility toggles cause no document revision; setup edit does. Windows GUI checks icon states, keyboard, captions, 100%/200% DPI, narrow width and recorded 30fps path p95 frame interval ≤33.3ms on named machine/scene. Headless timing is not this evidence. |

Future implementation packet may list `include/nect/core.hpp`, `src/core.cpp`, `src/history.cpp`, `src/io.cpp`, a **new** versioned native schema and focused core/IO tests for authored definitions; `src/desktop/canvas.cpp`, `src/desktop/window.cpp` and focused Canvas/Window tests for gesture/UI; and `src/desktop/host.cpp` only if recovery/readback needs a change. The first authorized packet must freeze actual baseline SHA, allowed writes, data/version shape, reference and tie rules, error codes, fixture bytes/tolerances, and runtime acceptance before code. This revision authorizes edits to this design packet only.

**Owner decision queue / gate state:** `OWNER` must accept or amend (1) independent Margin/Grid bounds and Artboard resize behavior/preset scope, (2) tie priority and hidden-overlay snap behavior plus baseline coverage, (3) Guide and key-object/Artboard/Grid distribution meanings and Candidate REQ-95 adoption, (4) Nect defaults for AE collisions, and (5) the minimum P02-D adapter set. Until those are resolved in the Notion product owner and read back here, status remains `DESIGN_GATE / OWNER_DECISIONS_PENDING`; this is a reviewable proposal, not a frozen implementation contract.
