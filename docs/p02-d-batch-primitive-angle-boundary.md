# P02-D Packet B — Whole-object Polygon/Star rotation batches

Status: CLOUD AUTOMATED QUALIFIED; exact-candidate Linux/Xfce real-GUI replay and Windows/MSVC validation remain pending.

Base: `f24e33c4006bda894101d5276e5f68a14e9aeca0` / tree `659bbddff437d6f11606b05ee13c91a59cc560d2`.
Authority: [Packet B](https://app.notion.com/p/3eafd279a6f38185bce6e73fcb46b0df) and confirmed [REQ-162](https://app.notion.com/p/3e0fd279a6f3810ab72acc0d73a6c2ac).

Packet B/REQ-162 require the dial to share the exact numeric property Ref. This slice's common-relative-delta behavior is the authorized adapter policy, consistent with existing absolute and `+=`/`-=` numeric semantics and preserving each source's rotation difference; it is not wording explicitly stated by REQ-162.

## Bounded implementation

- Adds a dial only after the existing `generator.rotation` row when every selected whole object has a valid rotation Ref and its source type is Polygon or Star. Mixed Polygon+Star selections are supported. Other source types, point selections, and selections without that exact common numeric row get no dial.
- The dial and numeric editor carry the same complete, ordered Ref vector. The dial applies one common relative delta from each captured starting value, through one `EditProperties{targets, delta, true}` preview command; repeated previews recompute from the original snapshot. Exact equality alone gives a Common display, so values such as 5° and 365° stay Mixed.
- Absolute numeric entry and numeric `+=`/`-=` continue through the existing editor and Session path. The common exact number remains 17-digit round-trip text. Visible state/count/`+X zero`/relative-Δ captions use a bounded 7-significant-digit live delta; accessible descriptions and exact numeric values retain the full 17-digit signed delta and unwrapped angle.
- The shared point/primitive batch dial helper freezes Session/document/revision, selection and active frame, ordered Refs, and gesture generation. Point and primitive adapters keep separate domain validation. Primitive validation freezes each object/source ID/type/version and the exact `generator.rotation` Ref/value; a driven, stale, invalid, or out-of-range target rejects the whole batch. Inspector rebuild, close, disposal, dirty numeric drafts, and stale callbacks cannot restart or cancel a replacement gesture.
- Batch preview returns to an empty command list for no-motion/no-net gestures; a genuine 360° drag commits. The point target membership check uses explicit loops to avoid the reported MSVC 19.29 nested generic-lambda C1001 path without changing identity checks.

## Focused acceptance

`primitive_batch_angle_ui_tests` reports 58 checks; `batch_point_angle_ui_tests` reports 105. The primitive suite covers same-type and mixed Polygon/Star selection, exact ordered Refs, common/mixed and long-fraction values, numeric absolute/relative semantics, live delta, one revision/Undo/Redo/native reopen, Escape/no-motion/no-net/360°, later-target range rollback, driven binding/expression, stale source ID/type/revision/Host Session, dirty drafts, replacement gesture ABA, rebuild/close and reuse.

The shared geometry coverage checks common/mixed sizing and fixed-global non-cardinal samples `(16,0) → (14,8) → (8,14) → (0,16)`, with layout events after every point. It asserts stable dial/row geometry, a +90° result, visible `+X zero`/live-Δ text with no clipping, and exact full-precision accessibility. Dedicated point and primitive CTest cases run at `QT_SCALE_FACTOR=1.25` and assert actual widget DPR ≥1.2; normal and DPR2 cases remain.

Pre-repair offscreen geometry-only red evidence (`primitive-batch-geometry-red.log`) showed a Common row height of 60 versus Mixed 44 and an 8px center difference at 1000×650. That records content-dependent Common/Mixed sizing; it is not evidence by itself of the separate Linux/Xfce own-cloud GUI reproduction of the wrong mixed-angle delta. The new fixed-global arc passes after repair. Exact-candidate Linux/Xfce GUI replay and Windows/MSVC validation remain separate gates.

## Verification and limits

- Focused angle CTest: 12/12 pass, including primitive batch, point batch, their 1.25x and DPR2 cases, and single primitive/point plus Repeater regressions.
- All-target desktop Release build: PASS.
- Full CTest: 86 tests, 59 passed, 1 skipped, 26 failed. The 26-name failure set is identical to the recorded 82-test baseline (55 passed, 1 skipped, 26 failed); Windows DirectWrite/WIC/drive-path and host/process/MCP-dependent failures remain. This is not a whole-suite pass.
- `scripts/smoke.py --exe build/d0-core/nect`: PASS for fresh-process native validation and SVG anchor checks; it reports `mcp_tested=false` and `gui_tested=false`.
- No exact-candidate real-GUI, Windows/MSVC, or formal MCP result is claimed here.

Evidence logs under `build/d0-evidence/`:

- `primitive-batch-final-focused-ctest.log`
- `primitive-batch-final-all-build.log`
- `primitive-batch-final-full-tests.log`
- `primitive-batch-final-smoke.log`
- `primitive-batch-geometry-red.log`
- `primitive-batch-geometry-green.log`
- `primitive-batch-point-geometry-green.log`
- `primitive-batch-point-test-139-diagnosis.log`

SHA-256 of the reviewed source/test candidate:

- `src/desktop/window.cpp`: `5e870364e05702940ba5d392915a84b975126ac44b1dd0c19961d706760b5b33`
- `src/desktop/window.hpp`: `0186a0a6463f93d320bcb5fc4fcce142a2177de3d5574ec57708bc30203f15b6`
- `CMakeLists.txt`: `16a253b93c9b123fdb095f33bea52a45935e09f67d94238c0fba9dc03be8df82`
- `tests/batch_point_angle_ui_tests.cpp`: `22c47170684e9610bb725650d34a997afc62616598aaa2497c1cd6150e0596d8`
- `tests/primitive_batch_angle_ui_tests.cpp`: `3f0dabdb3b58f781cf80120c7f47c77cf2b41ff3ff7809e1d1b1553f586275a3`

SHA-256 of final evidence logs:

- Focused CTest: `0c2f26705de51b4d162dd6fedcdf3b06d7b9641882ccb7b7488519abedf1cbbd`
- All-target build: `6eac988101b75c6d3d2980441dae2b2f76fb962f85a22e77d71f52cc6117cfb5`
- Full CTest: `126dc75309276d693b6e87610f2691e068b6df9dc82397cbef538b2d19ee0325`
- Smoke: `7cd237678e4001117718f685f4c612cbbbcb217b79273c1ad43af6dfc8986cb4`
- Pre-repair geometry red: `7d0c25ead34b232eb860ce39f82d0405c621a2f4f344896a6faabec8a0bde6d9`
- Repaired primitive geometry: `fa6e6674e7296f251399cf1afaced46e581b0d9963748dc7e3619a043e12556e`
- Repaired point geometry: `5999aaa289f347d7adef7984e21fcaa6cbdfd25a73924cc41a6f80eece22a638`
- Point-test exit-139 diagnosis: `047e307c6526fd517f5b654a5c289fe2bb2fbbb8c452d7e4b94b1c8b21543c81`

No core/schema/transport/native-format changes, commit, push, CURRENT_GOAL edit, or broader REQ-162 completion claim is included. Matched Repeater batches remain a separate slice.
