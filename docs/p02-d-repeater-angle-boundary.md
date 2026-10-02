# P02-D Packet B — Repeater authored-angle adapter

Status: LOCAL CLOUD AUTOMATED QUALIFIED. Exact-candidate real-GUI and Windows replay remain pending; formal MCP parity is tracked in its separate test packet.

Base: `740cd0b48f258fd95fe4419bd61e6b0127bf1876` / tree `7df324245c55aeb6fd200106cceab9320b36107b`.

Authority: [Packet B](https://app.notion.com/p/3eafd279a6f38185bce6e73fcb46b0df), confirmed [REQ-162](https://app.notion.com/p/3e0fd279a6f3810ab72acc0d73a6c2ac).

## Bounded implementation

This slice qualifies the existing single Repeater rotation dial. It preserves its object name, exact operation Ref, zero-at-top/modulo-360 indicator convention, exact signed unwrapped degrees within ±1e9, adjacent numeric editor, and successful one-gesture interaction. It does not migrate point/handle or batch-angle controls.

Repeater now resolves the numeric editor from the parent property panel, shares a narrow angle adapter with the already-qualified primitive control, and marks only the Repeater rotation input for 17-digit round-trip display. Target validation freezes the document and Host Session identity, revision, object source identity, operation ID/type/version, exact rotation Ref, and driven-source state. Active preview ownership includes the transient Session gesture generation, so a stale callback cannot update, commit or cancel a same-revision replacement gesture.

The adapter keeps a small cancellation collection rather than one replaceable callback. `QPointer` lifetimes and synchronous cancellation/disarm cover Inspector rebuild, close and destruction. A whole-method Inspector reentrancy guard prevents focus-out `editingFinished` from recursively rebuilding/deleting the same layout; the numeric edit itself still follows its ordinary Session commit path, and the outer rebuild reads the resulting current Session state.

Repeater keeps its established range policy: an out-of-range sample displays the last valid preview and leaves the gesture active for recovery or release. Its last-valid marker and dial/numeric baseline reset only after a new gesture is admitted. A modified numeric draft refuses dial begin and remains untouched. No-motion and floating-residue no-net drags clear their own preview without creating History; a real drag remains one revision/Undo/redo.

## Evidence

Test-first baseline: the new `repeater_angle_ui_tests` target built and failed against the pre-repair source at `Live numeric field previews exact Repeater dial value`, proving the form-layout-only lookup returned no live numeric editor.

The final dedicated Repeater test passes 67 checks in the offscreen normal-DPI run, including the zero-at-top rendered indicator; exact numeric/dial Ref and long-fraction text; invalid-expression draft retention; dirty-numeric refusal; signed/multi-turn values; one-revision commit/Undo/redo; no-motion and floating-residue traces; busy and same-revision ABA replacement gestures; stale Host Session/document/revision/source/operation; expression/binding disablement; last-valid range recovery, including Escape then reuse; coexisting primitive/Repeater dial noninterference; focus-out commits across selection change and same-selection refresh; and rebuild/close teardown followed by reuse.

Final focused CTest: 6/6 passed: `repeater_angle_ui_interaction`, `repeater_angle_ui_hidpi`, primitive113/114, and Utility277/278. The high-DPI test sets `QT_SCALE_FACTOR=2` and checks the actual widget DPR. Final all-target desktop Release build passed, including `nect_desktop`.

Final full regression: 78 tests, 51 passed, 1 skipped, 26 failed. The failing-name set matches the saved 76-test primitive-angle baseline exactly; these failures are retained rather than counted as a full-suite pass. This Linux run remains blocked on existing Windows DirectWrite/WIC/drive-path requirements and related process/MCP host checks.

Final `scripts/smoke.py --exe build/d0-core/nect` passed: fresh-process native validation and SVG anchor checks; it explicitly reports `mcp_tested=false` and `gui_tested=false`.

One pre-guard CTest run recorded a Repeater interaction SegFault; a focused passing run also emitted `QLayout: Attempting to add QLayout ... which already has a layout`. The working hypothesis was a nested Inspector rebuild when a focused dirty numeric editor emitted `editingFinished` during teardown. Core dumps were disabled and gdb/lldb were unavailable, so no crash stack was captured and causal attribution is not claimed as stack-proven. The final guard regression forces that focus-out/rebuild sequence, proves the numeric edit commits once and the rebuilt numeric/dial values remain current, and the final focused/full runs emit no such QLayout warning and the Repeater normal/high-DPI tests pass.

Ignored local evidence under `build/d0-evidence/`:

- `repeater-angle-final-all-build-resumed.log` SHA-256 `63cab28dc570a984628958ebc0aa9b6788ca2e5e97d7db0422f9eefb630cae9f`
- `repeater-angle-final-focused-ctest.log` SHA-256 `5e13dc249004813d4d84ab6295c42ff2d0630d732d0e7d1506e7a1f6d3a705ec`
- `repeater-angle-final-full-tests.log` SHA-256 `82df3a43e89115c34dc52a2431ddbc0f89dac254b4196a8a124bc00f88747feb`
- `repeater-angle-final-smoke.log` SHA-256 `7cd237678e4001117718f685f4c612cbbbcb217b79273c1ad43af6dfc8986cb4`
- Pre-guard CTest failure retained at `repeater-angle-full-tests.log`; verbose reentrancy-warning sample at `repeater-angle-ctest-verbose.log`.

No commit, push, CURRENT_GOAL edit, native/schema edit, external write, actual GUI or formal MCP completion is claimed. Whole REQ-162 remains open.
