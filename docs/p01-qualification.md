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
| R2 Radial / Gradient / Repeater / Expression | PASS | Actual Windows GUI 13/13: Circle source, Stroke OFF, linear Fill stops, explicit Repeater Anchor (480,320)→(360,320), 12→6→Undo 12 at fixed 30°, valid/invalid fx draft, gradient handle drag/Undo, native save/fresh-process reopen and SVG/PNG export. Sol independently checked the native source and exports. Receipt: `build/manual-recipes/p01-r2-20260924-200359-82ea8ecb/R2-receipt.json`. | Closed |
| R3 Text / Named Color | PASS | Actual Windows GUI 11/11: two editable Japanese Text objects, Yu Gothic selected and rendered, horizontal/vertical layout, shared Ink Named Color linkage, two-use color change/Undo, fixed-frame overflow with glyphs retained, native save/fresh-process reopen and SVG outlines/PNG export. Sol independently checked native Unicode/links and exports. Receipt: `build/manual-recipes/p01-r3-20260925-055123-3a809cfb/R3-receipt.json`. | Closed |
| R4 Mask / Group | NOT_EXECUTED | Pending; R4.png feeds R5. | Qualification Worker |
| R5 Linked image | NOT_EXECUTED | Pending R4.png. | Qualification Worker |
| R6 Artboard inheritance | NOT_EXECUTED | Pending. | Qualification Worker |
| R7 SVG intake / Ungroup / pivot | NOT_EXECUTED | Pending. | Qualification Worker |
| R8 Atomicity / Recovery | NOT_EXECUTED | Requires an owned dedicated Window, recovery directory and endpoint. | Qualification Worker |

## Failure ledger

No P01 product or manual finding has been accepted yet. R1 is an environment limitation, not a product failure: entry = a separate new Draw Path document after P, three clicks, Enter and one Undo; input = Alt+drag on the selected first anchor at r4; expected = symmetric in/out handle creation; actual = no attempt because the native GUI driver supports ordinary drag and key chords but no held-modifier drag; next owner = GUI automation capability or human manual qualification. Existing `tests/canvas_tests.cpp` exercises an Alt-modified drag in Qt, but that source/test does not substitute for this recipe's live manual gesture. R2 had one operator wheel error and one accessibility readback error, both recovered without a remaining product mutation; neither is a product finding. For later candidates record entry, input, expected, actual, revision, reproduction, owner/surface and receipt. Sol deduplicates one underlying gap across Recipes before assigning a bounded repair.

### R1 evidence and Sol review

- Baseline and runtime: `03219d440a540cb9d06e69f8e8d5e434a25bc5b2`, Release `nect_desktop.exe` SHA-256 `36DBD80439951364E4A7857586C7DB20705394C0A950087FA37DF0C325047739`, Windows window 1402×932. Binary/source parity is inferred from no production source changes since CP2 implementation; no exact compile-SHA manifest was found.
- GUI revisions: Curve parameters at r9; out.length 80→100 at r10, Undo 80 at r11, Redo 100 at r12. Circle source center (600,260), radius80 at r16. East x690 Point Edit at r17, disabled/east680 at r18, enabled/east690 at r19, reset/east680 at r20, Undo/east690 at r21.
- The saved native 0.13 document was read independently: two Curve points retain (100,160,out.angle −45,out.length100) and (360,160,in.angle135,in.length80); Circle retains source (600,260,r80) and an enabled East x690 override. Reopened GUI showed the Circle/override with a fresh revision. SVG XML parsed with viewBox `0 0 960 640`; PNG header parsed as 960×640, color type 6 (alpha).
- Separate GUI Draw Path: P, three clicks, Enter, then Ctrl+Z left two points. The unsaved document was closed normally; no Nect process remained. `SetValue` alone was a draft and was not counted as a committed edit; committing by focused text entry and Enter worked.
- Task-owned local artifacts: `build/manual-recipes/p01-r1-20260925-f392088f/`. SHA-256: receipt `FB9AFA4E77339A0622465A998DB26BA2B5882EE3DD58061983104FE0EE1A3B24`; native `46A4A369102D734325807114F7E2D14B1DC59EAB092D1E7306CDB6720B388B74`; SVG `B14491D0F96F3F401B7980A69897034348A3AFB64F21C43ED15A8C09D1C15438`; PNG `4D85EF61DD4622B5D4BEAFE157F3E91B2FE8B0933BFC3436279206E5C121AF2D`. These raw files are ignored local evidence; the compact decision record is this tracked document.

### R2 evidence and Sol review

- Baseline: `codex/practical-alpha@c9913f9e7a78a3338022caa1728e37e28d5c786a`; Release `nect_desktop.exe` SHA-256 `36DBD80439951364E4A7857586C7DB20705394C0A950087FA37DF0C325047739`. No production source changed after the build's CP2 source commit; exact compile-SHA manifest is still unavailable.
- Worker executed 13/13 GUI checks with no skipped step. It changed the Repeater's own Anchor X from the preset 480 to 360 while leaving Object Anchor at 480; GUI showed 12 copies around that pivot. Copies 6 kept the 30° interval, and GUI Undo restored 12. Radius fx previewed 25 while the committed source stayed 20, Apply committed `10 + 15`, and invalid `10 +` plus Cancel retained 25. Gradient End X drag committed once at r15; Undo restored 500 at r16. A fresh Release process reopened the saved file with the same visible properties.
- Independent native readback: version 0.13; Circle center (480,320), radius literal 20 with expression `10 + 15`; Stroke disabled; linear Fill with offsets 0/1 and requested sRGB stop colors; Repeater Copies 12, Position (0,0), Anchor (360,320), Rotation 30. Independent SVG parsing found viewBox `0 0 960 640`, 12 paths and 12 linear gradients. PNG header was 960×640 RGBA; visual inspection showed the 12-copy motif on transparency.
- Task-owned local artifacts: `build/manual-recipes/p01-r2-20260924-200359-82ea8ecb/`. SHA-256: receipt `2B24C2B152907CBC130A47E4A600513E83F37B919D2F37A264208B243A280055`; native `1DF952F438A6D08EF0E328048ED8A51FF6210A05C5F14EDC2770EE9394F82294`; SVG `5FB60EEF9650B03B840D76F3F184621A33823F5FB7F51B56058B5E9F168422DC`; PNG `AFA0A343972E849EF2B3F502EFEA92F6657DBF40BB603C4E05D3136E29D6D5B5`. The Worker closed both owned Nect processes; no process remained. The raw files are ignored local evidence.

### R3 evidence and Sol review

- Baseline: `codex/practical-alpha@b8ccec1c795c07ad3bac61df70d6dbf59b320fd9`; Release `nect_desktop.exe` SHA-256 `36DBD80439951364E4A7857586C7DB20705394C0A950087FA37DF0C325047739`; window 1402×932. The GUI selected and rendered `Yu Gothic`; installed Yu Gothic Regular/Medium/Light/Bold font files were checked.
- Worker passed 11/11 GUI checks. It authored `形と光の庭` followed by a newline and `Nect` in horizontal Text, duplicated the whole object, replaced the copy with `縦書きの庭` and set it Vertical. `Ink` changed from `#18303FFF` to `#BC465EFF` and both Text objects followed; Document Used Colors showed two uses and Undo restored the original. Document Used Colors and Copied History were separate tabs. A 48×48 Fixed frame showed `TEXT_OVERFLOW` with glyphs retained, then Auto size was restored before save. A fresh Release process reopened the native file and displayed the same body, direction, rendered font and linked Fill for both objects.
- Independent native readback: version 0.13, exact Unicode text, origins (100,100)/(700,100), horizontal/vertical Auto layout, font family Yu Gothic, one Ink Named Color and all four Fill channels on both objects bound to its stable ID. Independent SVG parsing found viewBox `0 0 960 640`, two path outlines and no `<text>` nodes. PNG header was 960×640 RGBA; visual inspection showed both text placements. Native retains editable Text while SVG is outlined.
- Task-owned local artifacts: `build/manual-recipes/p01-r3-20260925-055123-3a809cfb/`. SHA-256: receipt `DFC0978EADBD7677733544BF2B3339E89E34CC0C430F263F4D2C3F06A32D8818`; native `9E766B08EF43D7E68F948200424BC380B71739E8139DD983DC592CFA80D2C840`; SVG `789E3C55EFACF72D12F8F4B01340B896BA89D4DD4FE1A0688FA71F2FA5F9DA4F`; PNG `FDA5834B5B81F87B39AABFFB3B3ADBF35248F454C3B9613F1B409B36A391A7B5`. The Worker closed both owned processes; no Nect process remained. The raw files are ignored local evidence.

## P02 transfer

Prepare only after R1–R8 final classification. P02 Grid / Guide / Margin / Smart Snap and P02-D daily UI remain DESIGN_GATE; no P02 implementation is authorized in this checkpoint.
