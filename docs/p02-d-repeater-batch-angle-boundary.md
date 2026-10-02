# P02-D Packet B — Whole-object Repeater rotation batches

Status: LOCAL CLOUD AUTOMATED QUALIFIED. Exact-candidate Windows/MSVC caption requalification and actual GUI replay remain pending. The Text-target test case is Windows-gated because Linux text projection requires DirectWrite.

Base: `acc153464122beababbe47d7815bc89b0a39fd49` / tree `ea9a64597ff57ad5880ab0ca966d2fe6689561d5`.
Authority: [Packet B](https://app.notion.com/p/3eafd279a6f38185bce6e73fcb46b0df) and confirmed [REQ-162](https://app.notion.com/p/3e0fd279a6f3810ab72acc0d73a6c2ac).

## Bounded implementation

- Adds a whole-object dial only when every selected Path or Text object has an ordinary `nect.shape.repeater` v1 at the same stack slot, with its own `rotation` Scalar and evaluated Ref. Path and Text source kinds need not match; Macro entries and point selections are outside this adapter.
- The dial is attached to the existing stack `rotation` numeric row. Both carry the complete ordered Ref vector, built from each object's own operation ID. Multiple matching Repeater slots are independent controls. Repeater parameter rows are guarded before numeric construction when a parameter is absent on any selected Repeater.
- Each object freezes its own operation ID/type/version and slot, full ordered stack identity signature, object kind, primitive/Text source identity, full rotation Scalar/driver state, and committed value. The validator also freezes ordered selection/Refs, Host Session/document/revision, and active composition/artboard, and checks committed values before begin, preview and commit. Stack signatures are compared per object; unrelated entries do not have to match between objects.
- Rotation binding/expression state alone disables the batch. An operation-enabled driver does not make the rotation driven, and a disabled ordinary Repeater remains editable. Existing numeric absolute and `+=` semantics are unchanged.
- The batch applies one `EditProperties{all_refs, delta, true}` per preview, recomputed from the committed snapshot. Exact equality defines Common; 5° and 365° remain Mixed. Numeric and accessible authored angles remain exact/unwrapped through ±1e9; only the indicator wraps modulo 360.
- Repeater uses top zero and clockwise-positive motion throughout paint, caption, tooltip and accessibility. Point/primitive dials retain +X zero. The shared angle row remains fixed at 80px; the caption gets all width left after the dial instead of competing with an empty trailing stretch.
- A range failure on any target cancels the complete Repeater batch gesture. The existing single-Repeater dial keeps its last-valid/recoverable range policy.

## Focused acceptance

The dedicated test covers different operation IDs, two matching slots, per-object stack signatures that differ outside the target entry, missing/mismatched Repeater rows, point-selection exclusion, same-revision operation/source/order/rotation-Scalar replacement refusal, exact Common/Mixed state including 5° versus 365°, numeric precision and absolute/`+=` behavior, noncumulative −355/5/725 previews with +8/+12/+10 resolving to −345/15/735, and Path/Text preview/commit under Windows.

It also covers rendered top-zero indicator orientation, normal/DPR2/1.25 geometry, fixed-global top-to-right and noncardinal arcs for Common and Mixed captions, visible-delta/accessibility precision and clipping, ±1e9 boundaries, later-target binding/expression/range rollback, operation-enabled driver vs rotation driver, disabled ordinary operations, Escape/no-motion/out-and-back/±360°, dirty-draft refusal and focus-out commit once, busy foreign gestures, stale preview/release/Escape ABA, rebuild/disposal/close/reuse, exact one-revision Undo/Redo, whole-document preservation outside the target Scalars, native save/reopen, and save safety after failed preview.

## Verification and limits

- Dedicated Repeater batch suite: 3/3 CTest cases passed (normal, DPR2, fractional 1.25 geometry); direct offscreen run: 68 Repeater batch checks passed. The Path/Text case is compiled/run only on Windows; Linux throws `TEXT_PLATFORM_UNSUPPORTED` while projecting Text.
- Existing point batch, primitive batch and new Repeater batch focused angle cases: 9/9 passed.
- All-target desktop Release build: PASS.
- Full CTest: 89 tests, 62 passed, 1 skipped, 26 failed. The failed-name set is exactly the same 26 names as `integrated-primitive-q1-full.log`; this is not a whole-suite pass.
- `scripts/smoke.py --exe build/d0-core/nect`: PASS for the M0 headless real-process workflow, fresh-process native validation and SVG anchor checks. It reports `mcp_tested=false` and `gui_tested=false`.
- Windows caption fit: the previous independent Windows run found a Common caption height-for-width of 124px against an 80px row. This candidate gives the caption all remaining row width and preserves the 80px fixed height. The exact patched Windows/MSVC rerun is still pending.
- No exact-candidate actual GUI or formal MCP acceptance is claimed. The earlier immutable `556b122` Linux GUI replay exercised the point and Polygon/Star primitive dials, not this Repeater batch candidate.

## Candidate and evidence hashes

- `src/desktop/window.cpp`: `e415c51506158d21816a797a9b6387e81b282d5c1a51b63e1f06a56bb283426c`
- `src/desktop/window.hpp`: `3dcfabc249f3f501ea83f9c18e8dbeb57d5e4ea8b2978ba21fd38176b7a47dec`
- `CMakeLists.txt`: `bafa28b9dc2fbbd6293c3040567e93751e03c6c61a5ab76bf01c1d0ff8e7c49c`
- `tests/repeater_batch_angle_ui_tests.cpp`: `09e0b616e104250389f5343abc63fc91d91e2be5024a8f763efeeada738624ae`
- Final all-target build log: `build/d0-evidence/repeater-batch-final-all-build.log` — `ed69de4750ca6d59f07c01a429340f889a0f52d6911ee204585346d8e8dec8ca`
- Final focused angle CTest log: `build/d0-evidence/repeater-batch-final-focused-angle-ctest.log` — `65fed7b057b6f7f4e7e382edb47ee917b6033a59c389712908b54db5527fb701`
- Direct focused checks: `build/d0-evidence/repeater-batch-final-focused-checks.log` — `082eebfe286ad49d649bb7beaa063b04c577d83e34f68906aedec2b1f3246911`
- Final full CTest log: `build/d0-evidence/repeater-batch-final-full-tests.log` — `dd5d39d5c0f09af2535460d1788269abe854529a6786ea9b91fed6feee038b70`
- Final smoke log: `build/d0-evidence/repeater-batch-final-smoke.log` — `7cd237678e4001117718f685f4c612cbbbcb217b79273c1ad43af6dfc8986cb4`

No commit, push, `CURRENT_GOAL.md` edit, core/schema/native/API/transport change, or broader REQ-162 completion claim is included. This receipt covers only the bounded local source/build slice; Windows and exact-candidate GUI gates remain separate.

## Integrator rerun

Root independently reran all-angle normal/high/fractional scale cases plus storage/protection:17/17 passed. Log `build/d0-evidence/repeater-batch-root-focused.log`, SHA256 `ae92fc99c0b16a3d8de567fa550c66ea2ba2dfe0a0b42d88f1dc259cc659b8aa`. This does not change the Windows/actual-GUI boundaries above.
