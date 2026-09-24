# P01 — current contract and operation qualification

## Authority and baseline

- Mission: `Nect/practical-alpha`, P01 QUALIFICATION. Current user authorization: 2026-09-25.
- Source: Notion [Worker packets revision 3](https://app.notion.com/p/3e3fd279a6f381ee8ab0ea5ea614c006), [R1–R8 recipes / 083b53d](https://app.notion.com/p/3e3fd279a6f381d086c6c323161142cd), Nect Home.
- Git start: `codex/practical-alpha@03219d440a540cb9d06e69f8e8d5e434a25bc5b2`, equal to `origin/codex/practical-alpha` at fresh read; clean before Mission Brief reconciliation.
- Prior Stroke CP2: `CLOSED / REMOTE_SYNCED`. Its stop boundary applied to that packet. P01 start is separately authorized.

## Decision rules

`PASS` requires the real semantic and GUI/native/export scope of the recipe. API setup, Qt tests and older receipts are identified separately. `FAIL_IMPLEMENTATION`, `FAIL_UI`, `FAIL_MANUAL`, and `BLOCKED_ENV` require exact observed conditions and a next owner or missing capability. No recipe becomes PASS by skipping a failed or unrun step. Original examples and user documents remain untouched.

## Recipe status

| Recipe | Final class | Evidence and remaining subchecks | Next owner |
| --- | --- | --- | --- |
| R1 Curve / Circle / Point Edit / History | BLOCKED_ENV | Actual Windows GUI: Curve values, Undo/Redo, Circle source/Point Edit toggle/reset/Undo, native Save As/reopen, SVG/PNG, and Draw Path P/3 clicks/Enter/Undo passed. Additional Alt+drag was not executed because the available `@oai/sky` driver has no held-modifier drag. Receipt: `build/manual-recipes/p01-r1-20260925-f392088f/R1-receipt.json`. | GUI automation capability or human manual Alt+drag qualification |
| R2 Radial / Gradient / Repeater / Expression | NOT_EXECUTED | Read-only source map confirms the controls exist; preset Repeater Anchor starts at Circle center (480,320), so the recipe's (360,320) requires an explicit edit. Existing focused tests cover components, not the combined live motif. Pending R1 GUI writer release. | Qualification Worker |
| R3 Text / Named Color | NOT_EXECUTED | Pending. | Qualification Worker |
| R4 Mask / Group | NOT_EXECUTED | Pending; R4.png feeds R5. | Qualification Worker |
| R5 Linked image | NOT_EXECUTED | Pending R4.png. | Qualification Worker |
| R6 Artboard inheritance | NOT_EXECUTED | Pending. | Qualification Worker |
| R7 SVG intake / Ungroup / pivot | NOT_EXECUTED | Pending. | Qualification Worker |
| R8 Atomicity / Recovery | NOT_EXECUTED | Requires an owned dedicated Window, recovery directory and endpoint. | Qualification Worker |

## Failure ledger

No P01 product or manual finding has been accepted yet. R1 is an environment limitation, not a product failure: entry = a separate new Draw Path document after P, three clicks, Enter and one Undo; input = Alt+drag on the selected first anchor at r4; expected = symmetric in/out handle creation; actual = no attempt because the native GUI driver supports ordinary drag and key chords but no held-modifier drag; next owner = GUI automation capability or human manual qualification. Existing `tests/canvas_tests.cpp` exercises an Alt-modified drag in Qt, but that source/test does not substitute for this recipe's live manual gesture. For later candidates record entry, input, expected, actual, revision, reproduction, owner/surface and receipt. Sol deduplicates one underlying gap across Recipes before assigning a bounded repair.

### R1 evidence and Sol review

- Baseline and runtime: `03219d440a540cb9d06e69f8e8d5e434a25bc5b2`, Release `nect_desktop.exe` SHA-256 `36DBD80439951364E4A7857586C7DB20705394C0A950087FA37DF0C325047739`, Windows window 1402×932. Binary/source parity is inferred from no production source changes since CP2 implementation; no exact compile-SHA manifest was found.
- GUI revisions: Curve parameters at r9; out.length 80→100 at r10, Undo 80 at r11, Redo 100 at r12. Circle source center (600,260), radius80 at r16. East x690 Point Edit at r17, disabled/east680 at r18, enabled/east690 at r19, reset/east680 at r20, Undo/east690 at r21.
- The saved native 0.13 document was read independently: two Curve points retain (100,160,out.angle −45,out.length100) and (360,160,in.angle135,in.length80); Circle retains source (600,260,r80) and an enabled East x690 override. Reopened GUI showed the Circle/override with a fresh revision. SVG XML parsed with viewBox `0 0 960 640`; PNG header parsed as 960×640, color type 6 (alpha).
- Separate GUI Draw Path: P, three clicks, Enter, then Ctrl+Z left two points. The unsaved document was closed normally; no Nect process remained. `SetValue` alone was a draft and was not counted as a committed edit; committing by focused text entry and Enter worked.
- Task-owned local artifacts: `build/manual-recipes/p01-r1-20260925-f392088f/`. SHA-256: receipt `FB9AFA4E77339A0622465A998DB26BA2B5882EE3DD58061983104FE0EE1A3B24`; native `46A4A369102D734325807114F7E2D14B1DC59EAB092D1E7306CDB6720B388B74`; SVG `B14491D0F96F3F401B7980A69897034348A3AFB64F21C43ED15A8C09D1C15438`; PNG `4D85EF61DD4622B5D4BEAFE157F3E91B2FE8B0933BFC3436279206E5C121AF2D`. These raw files are ignored local evidence; the compact decision record is this tracked document.

## P02 transfer

Prepare only after R1–R8 final classification. P02 Grid / Guide / Margin / Smart Snap and P02-D daily UI remain DESIGN_GATE; no P02 implementation is authorized in this checkpoint.
