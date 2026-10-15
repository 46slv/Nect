# R03 / REQ-33 evidence boundary after axis-only baseline decision

**Audit date:** 2026-09-26. **Source:** `codex/practical-alpha@3eeb3656e49342e2ec6423ac2fea35ed4d35bb57` and the linked P02/R03 packets; this is an evidence classification, not a new runtime qualification.

| Route obligation | Current evidence | Remaining boundary |
| --- | --- | --- |
| Authored Grid/Guide/Margin and object/point Snap | P02-A/B/C1/D exact command, Canvas, GUI/API/native fixtures; C1 serial 41/41 and owned cold reopen | Full R03 acceptance still depends on combined daily-use and host rows. |
| Reference Align/Distribute and explicit spacing | P02-C2 core/API/GUI/Undo/native cold reopen | 200% interactive GUI remained BLOCKED_ENV. |
| Equal-gap | R03-ENTRY-01 positive/negative one-sided and prior two-sided fixtures; owned GUI/API/native cold reopen | No need to replay those valid PASS rows absent a relevant code change. |
| Measured horizontal multiline Text baseline | R03-BASELINE-01/02 focused Canvas/Text, feedback, Undo, owned native/desktop readback; first slice also had genuine GUI feedback/drag | Later moving-baseline interactive GUI remained BLOCKED_ENV. |
| Measured quarter-turn horizontal and vertical-writing column baselines | R03-BASELINE-03/04 focused Canvas/Text, native encode/decode, Release and serial 41/41, DPR2 Canvas, bounded visible timing; additional vertical Text API/native independent cold reopen below | Interactive Snap drag NOT_RUN for these slices. The independent reopen proves vertical authored Text placement persistence, not GUI Snap. Timing was not dense vertical-writing stress. |
| Arbitrary-angle Text baseline | 2D line geometry assessed in `r03-oblique-baseline-decision.md` | User selected axis-only behavior for now. Oblique Snap is DEFERRED, so REQ-33 stays partially open. |

The R03 Route exit requires fixed coordinate oracles, cancel, save/reopen, UI reachability and API parity. The local packets support individual slices; they do not prove the combined R03/REQ-33 or L1 acceptance. The existing Computer Use empty app/window inventory remains BLOCKED_ENV; neither Qt tests nor API/native readback turns it into an interactive GUI PASS. Do not repeat an identical inventory attempt without new host/capability evidence.

**Additional vertical native readback:** Owned `build/Release/nect_desktop.exe` SHA-256 `5D2B536EAF98280669549FB8E8688F635469B0F99EDB0BDD8F5225FF5D62682D` opened a fresh Session `9c68fd66-48a4-4292-835b-841662490084`. The local Session API created one authored `direction=vertical`, two-column `H\nH` Text with 80-du line spacing, applied exact numeric `TranslateObjects(dx=16,dy=0)` as one revision 2 transaction and saved `build/r03-vertical-native-live/vertical-text.nect`. A distinct desktop process and Session `52aeb49a-2c3a-45ab-85d4-0f7318414c72` cold reopened the same Document `5540daed-0f2f-4ff2-8d74-1932f6dbce3a` at revision 0; API inspect returned `direction=vertical`, `tx=16`, `ty=0`. Native SHA-256 before and after reopen was `4E591B86FEE5B4687F374A0F2C2576B30C0DF421FD9559B5E4524209FFB931B8`. The ready/API receipts and script are ignored under `build/r03-vertical-native-live/` and `build/r03-vertical-native-api.py`. Both owned desktop processes were closed. This validates authored vertical Text persistence only; it does not validate a GUI Snap drag or the measured baseline metric after cold reopen.

**LOCAL_WAIT / R03 interactive GUI row:** Actual vertical/quarter-turn baseline drag, visible feedback and Undo need an owned, controllable Windows Nect window. The last Computer Use inventory exposed no app/window target (`BLOCKED_ENV`); it does not invalidate the focused Canvas/Qt or desktop API/native proof above. Resume when a supported app/window inventory and control path becomes available; do not retry the same inventory blindly. This row does not block independent Route work. R02 typed dependency/save and R11-I retain their own DESIGN_GATE boundaries.

## 2026-10-16 native external-driver follow-up

The historical empty-inventory wait above is superseded for two exact native
Window fixtures. Cua Driver 0.32.0 launched and observed the owned production
`Window`/`Canvas` linked from the unchanged accepted source
`8443e695d11edb07e266ca938324e1b5bf1a5f74`; this was a live Windows/Qt 6.5.3
window driven by external input. No product source repair or replay of the old
Canvas/Inspector test suites was needed.

Before constructing each Window, the owned probe independently formed full
native-valid original, expected and foreign documents, measured the two Text
baselines and established an exact 16-du X difference. A normal typed
`TranslateObjects` Session established the complete expected Document and
History. The Window used only its owned INI, FolderLibrary and recovery paths.

| Native producer | Observed result |
| --- | --- |
| Vertical-writing first column, PID 82624 / HWND 44572136 | External 15-pixel horizontal drag at zoom 1 committed `tx=16` in revision 1. During the gesture, the native Window displayed its guide and `X: column 1 baseline Text baseline → target-text column 1 baseline`. |
| Horizontal Text rotated +90 degrees, PID 84492 / HWND 7151530 | External 15-pixel horizontal drag at zoom 1 committed `tx=286` from `270`, in revision 1. During the gesture, the native Window displayed its guide and `X: first-line baseline Text baseline → target-text first-line baseline`. |

In both cases the entire committed Document and typed History matched the
independent oracle. Actual Toolbar Undo restored the full original native
document with no further Undo available; Redo restored the full expected
document and History. Selection, zoom and Host identity stayed fixed. Saved
bytes matched the full expected native bytes; a distinct Host in the same
process reopened the same complete document. Both owned processes exited and
the exact `Local\NectBuild` mutex was free, non-abandoned, released and disposed.

The driver accepted a requested background drag but reported its delivery as
unknown, route `global_input`, and effect `unverifiable`. The result above is
established by the actual native semantic records and screenshots, not that
transport status. This is external-driver native GUI evidence, not physical
hand or subjective acceptance. The foreign document was an independent oracle;
no foreign/stale producer was exercised. Cancel and negative exclusions retain
their prior original test bindings. Different DPI/fonts, cold-process reopen,
dense vertical typography, arbitrary-angle Snap and combined R03/REQ-33
acceptance remain outside this follow-up. Mission accounting stays 43/89 and
REQ-16/42/209/210 remain open.

Receipt: `C:/Users/shiro/.codex/scratch/01a1413d-d9e8-7c22-b8f0-df9aa1b56bed/r03-native-baseline-checkpoint.json`,
SHA256 `AE67D6011D799CF65A97A7B4343200FBB7AC44A6F4B5A3B1722B91AD68E6FC1E`.
The receipt binds the probe source, binary, original production libraries,
63 artifacts, live gesture records and both native feedback screenshots.

## 2026-10-16 scoped Guide point reentry repair

An original native Window then exposed an uncovered REQ-32 precision-placement
failure. Its Artboard spans Y=100..400, local X Guide is 50 and global Y Guide
is 400. A raw point drag from `(38,350)` to `(49,405)` should finish at
`(50,400)`: the Y correction puts the point back on the local Guide's segment.
Before repair, PID 29308 committed `(49,400)` and dropped the X feedback.
Full native-valid expected/original/foreign oracles existed before Window
construction; the full committed native differed from that expected oracle.
The frozen original RED receipt is `point-segment-red.json`, SHA256
`DE0B64DFABFEA24A1CEF32603508322BB8D39AAB6C0E72193FA73D5A498C1BC9`,
under the same owner scratch directory as above.

Four added lines in `Canvas::snap_delta` re-evaluate X candidates using the Y
correction before the existing segment-rejection loop. Threshold, tie priority,
frozen geometry, core commands and authored format are unchanged. The exact
same external driver/probe and native oracle, relinked to the repaired Canvas,
committed `(50,400)` in PID 90644 and displayed both Guide identities. The full
native Document and typed History matched; actual Toolbar Undo/Redo restored
the complete originals/expected result, and exact saved bytes plus fresh
same-process Host readback matched. Both owned processes closed normally.

Focused Canvas checks passed 60 checks: both axis orders, both corrected
outside-segment cases, complete native/History, one Undo/Redo, cancellation and
the existing clipping cases. The affected baseline regression passed 110
checks. Registered CTest `canvas_guide_segment_reentry` passed 1/1. Release
Canvas tests/Desktop and the unchanged native probe relink passed. Initial
test startup `C0000135` was an existing Qt6Test.dll PATH issue; using the
existing Qt runtime resolved it, and the new CTest carries that runtime PATH.
No dependency or user-setting change was made.

Repair receipt: `point-segment-checkpoint.json`, SHA256
`1F225569F839479E93FA37B4B9232BED52AE9A74A9B0C357F3A8687B240FE1B2`.
The external native repair proof is the positive X case; symmetric, outside
and cancellation checks are focused Canvas tests. Cold-process/API-socket,
foreign/stale producer, physical/subjective and full Mission acceptance remain
unclaimed. The preceding baseline native receipt remains original at its
`8443e69` source binding.
+
## 2026-10-16 view Fit during an authored Circle drag

The original Windows/Qt 6.5.3 Window accepted a 20-pixel external radius drag
from radius 40 while its production Fit Artboard QAction was invoked after
the first preview. Fit changed zoom from 1 to 1.2703125; the committed radius
was 55.74415744157437 instead of the independently authored 60. Full native-valid
original, expected and foreign oracles plus typed Session/History were formed
before Window construction. Original RED: owner scratch `circle-fit-red.json`,
SHA256 `93F3891719FC82485C4878F675C67E0E3F7C3F6872C2B43EC70B0EC8F35213E2`.

Three lines in `Canvas::fit_bounds` retain the starting view while an owned
non-Guide gesture is active, matching interactive Zoom and Fit Selection.
Guide drag retains its existing frozen-inverse-view behavior. Ordinary Fit
outside a gesture still works. There is no command, source model or format change.

The same unchanged external probe and native oracles, relinked to the repaired
library, committed radius 60 in PID 49428 / HWND 20258674. Full native and typed
History matched. Actual Toolbar Undo restored the entire original with no
further Undo; Redo restored the full expected document and History. Saved
native bytes matched exactly and a separate Host in the same process reopened
the full expected document. The owned processes exited and the mutex was free.

Focused Canvas checks passed 61 checks, covering Center/Radius with Fit
Artboard/Fit All, full native/History, Undo/Redo/Escape, ordinary Fit outside the
gesture and the existing Guide frozen-view contract. Registered CTest
`canvas_fit_gesture` passed 1/1; Release Desktop/tests and probe relink passed.
An initial test fixture failed to center after resizing; its setup was corrected
to Fit then zoom 1, matching the original Window. Native RED probe/oracles did
not change.

The Fit QAction was triggered by the owned probe during the external Windows
drag; this is not physical keyboard evidence. Center, Fit All and cancellation
are focused Canvas evidence. Native gradient/pivot/body/point drags, different
DPI/fonts, socket API, cold-process reopen, whole CTest and whole Mission
acceptance remain unclaimed. Previous native evidence retains its original binding.
Receipt: `C:/Users/shiro/.codex/scratch/01a1415d-123f-7733-b5c6-ec3414f54d0f/circle-fit-checkpoint.json`,
SHA256 `972A1A5DA0E21E7DF8685B8AEDF6CA3EF1BBEA5DB60529EA89386F78C6812205`.

## 2026-10-16 Fit preserves an active marquee

An independent native Windows producer at `63b9856` confirmed a separate
selection failure. Before constructing the Window, the probe validated full
original/expected/foreign native documents and the exact view-only selection
oracle: rectangle `(70,90)` to `(130,150)` encloses Circle bounds `(90,110)` to
`(110,130)`. The expected authored document and Session History stay unchanged.
External Cua Driver input at zoom 1 dragged `(468,336)` to `(528,396)` over
2400 ms / 40 steps. After three native moves, the probe triggered the actual
production Window Fit Artboard QAction. In the original PID 41676 / HWND
21241854, Fit canceled the marquee and changed zoom to 1.2703125; release left
the selection empty. RED was sealed before the product repair in
`C:/Users/shiro/.codex/scratch/01a14170-4371-7870-ad99-6e1c4a8cb2b3/marquee-fit-red.json`,
SHA256 `C3B832B974FAB8FEA881034FAEE75570AE6187834A50EE8E815F6AE1367328FA`.

`Canvas::fit_bounds` now leaves an active marquee and its view intact, matching
the existing interactive Zoom/wheel guard. The former marquee-cancel branch is
removed. Authored drag protection and the Guide frozen-inverse-view exception
remain. The identical sealed probe/oracles and identical external input on the
relinked production library, PID 49352 / HWND 20520674, kept zoom 1 and selected
the stable Circle object. Full Document, preview, History, revision 0, empty
Undo/Redo, Host identity and the foreign oracle were preserved. Saved bytes
equaled the complete expected native bytes, and a distinct Host in the same
process reopened that same document. Both owned processes exited; the exact
mutex was free, non-abandoned, released and disposed; the Cua session ended.

Focused Canvas checks cover Object/Direct point marquee, Fit Artboard/All,
Escape, complete native/History preservation and normal Fit after release.
The affected combined Circle/Guide/marquee suite passed 97 checks and the
registered `canvas_fit_gesture` CTest passed 1/1. Release Desktop/tests and probe
relink passed. The first direct test launch lacked Qt6Test on its process PATH
and did not execute; using the existing Qt bin produced the genuine pre-fix
assertion RED (`expected zoom 1, got 1.40625`). No dependency changed.

Native positive evidence is Object marquee/Fit Artboard; Direct, Fit All and
Escape are Canvas checks. Fit is a probe-triggered production QAction during
external input, not a physical hotkey. Selection is view state and creates no
authored edit or undo entry. The driver reports unknown delivery/global input/
unverifiable effect; native records and screenshots establish the result.
Different DPI/fonts, cold-process reopen, socket API, full CTest and combined
R03/Requirement/Mission closure remain unclaimed.
Receipt: `C:/Users/shiro/.codex/scratch/01a14170-4371-7870-ad99-6e1c4a8cb2b3/marquee-fit-checkpoint.json`,
SHA256 `1F55A485DEEE6C3CD5A9C000964F18DC49C6269DA9D0C7C8A45A7ECABECDF88A`.

## 2026-10-16 marquee across a Session API Open

The production local Session API exposed a separate selection-context failure.
During an external Windows marquee drag, an owned QLocalSocket client requested
`open` through the real Host server after three native moves. The loaded file
retained the Document, Artboard and Circle IDs but changed the Circle name and
created a new Host identity. Open succeeded and initially left selection empty;
the old drag's release then selected the replacement Circle in PID 93336.
The ordinary Window menu cancellation remains valid; this is a different,
previously untested production socket consumer, not a direct Host-call bypass.

Before constructing the Window, the probe formed full native-valid original,
replacement and expected Documents plus independent Session/History and view
oracles. Expected selection after replacement is empty; the rectangle's fixed
world bounds enclose the colliding Circle. Original RED receipt: owner01a14186
scratch `api-marquee-red.json`, SHA256
`65067502285E893A76862F1A94957313D2479C83DDE1905220B290EA18D6112A`.
The original probe executable is independently preserved under that owner's
`api-marquee/original-probe.exe` with its sealed RED hash.

Canvas now captures the marquee's Document and Host at press. Refresh cancels
a stale marquee before projecting the replacement; move and release refuse
the expired context. This view-only cancellation preserves another authored
gesture. A same-Document edit remains compatible: revision alone does not
retire a marquee. No Session command, API protocol or native format changed.

The identical sealed probe, full oracles and external 2400-ms / 40-step drag,
relinked to the repaired production library, passed in PID 85388. The actual
socket Open changed Host identity, and release preserved empty selection,
the complete replacement native Document, History, revision 0, empty Undo/Redo,
foreign oracle and zoom 1. Saved bytes and a distinct same-process Host reopen
matched the complete expected native. Both owned native processes exited;
Cua sessions ended and the exact mutex was free and non-abandoned.

Registered CTest `canvas_marquee_context` passed 1/1 with 153 checks: Object
and Direct selection, Document and Host replacements, refresh and no-refresh
release paths, foreign authored preview/History preservation, same-Document
edit compatibility and affected current marquee/Fit/cancellation behavior.
The original focused assertion failed after two checks. Release Desktop/tests
and the unchanged probe relink passed. No valid older suite was replayed.

Native positive evidence covers Object marquee and same-Document/new-Host
socket Open. Different Document, Direct and foreign-gesture cases are focused
Canvas checks. The socket client lives in the owned probe process and uses
QLocalSocket; no separate external-process client is claimed. The inherited
probe field `fit_invoked` means the socket request was scheduled; no Fit action
executes in this probe. Selection creates no authored edit or undo entry.
Driver delivery remains unknown/global_input/unverifiable; native semantic
records and screenshots establish the result. Physical input, subjective
acceptance, DPI/fonts, cold-process reopen, full CTest and Requirement/Mission
closure remain unqualified.

Repair receipt: `C:/Users/shiro/.codex/scratch/01a14186-3867-7170-860b-5036a5957614/api-marquee-checkpoint.json`,
SHA256 `4C04CEEC3083A645C0A797C0EED61468816AF455F6E3EA9A97FA8A088D3B8575`.

## 2026-10-16 Fit preserves an active Hand pan

The unchanged production Window at `d1376c0` reproduced a view-mapping defect:
an external Windows Hand drag moved 80 by 40 pixels while the owned observer
triggered the actual Fit Artboard QAction after three native moves. Fit changed
zoom from 1 to 1.2703125; later pan moves reused the original press position,
leaving a clipped 695 by 600 Artboard interior instead of the independently
expected 637 by 478 interior at the original scale. Full native-valid original,
current, expected and foreign documents and typed History were fixed before
Window construction; the view translation oracle was saved before external
input. The original executable is preserved. RED receipt `pan-fit-red.json`
SHA256 `9BFE5A7313F0889D78F19F44E627D4D614BD1AACE4B1B0027A148D40A9E0A37F`
is in the current owner's scratch below.

Canvas `fit_bounds` now leaves an active pan's view unchanged. The same frozen
probe and identical 2400 ms / 40-step drag in PID 93256 / HWND 16457528 retained
zoom 1 and the expected full 80 by 40 translation. Committed and preview native
documents, revision 3, complete existing History with both Undo and Redo,
selection, Host and the independent foreign oracle remained unchanged. Save
bytes and a distinct Host in the same process matched the complete expected
document. A subsequent external accessibility invocation of the Toolbar Fit
changed zoom to 1.2703125 normally after release.

The original focused check failed at active pan scale after four checks.
Release Desktop/tests/probe builds, 199 focused Canvas checks and registered
`canvas_small_fit_contract` 1/1 passed. Canvas checks cover Hand/left and middle
pan with Fit Artboard and Fit All, continued movement, existing History and
normal Fit after release. Native evidence covers Hand/left with the actual
Window Fit Artboard action; that action is observer-triggered, not a physical
hotkey. Driver delivery is unknown/global_input/unverifiable; native semantic
records and screenshots establish the result. Wheel/explicit Zoom during pan,
native Space/middle, physical/subjective input, DPI/fonts, cold-process reopen,
full CTest and Requirement/Mission closure remain outside this slice. Owned
processes closed, Cua sessions ended and the exact mutex was free/non-abandoned.

Repair receipt: `C:/Users/shiro/.codex/scratch/01a141a1-dccb-7023-be1a-c40d6d01e11d/pan-fit-checkpoint.json`,
SHA256 `B15D130FB110E9AE7B7EFF81820C5916879F372A574699C1DC807947E8B74F71`.

## 2026-10-16 Utility Zoom reads back the accepted view

The unchanged Window at `6d29fd6`, PID 78016 / HWND 39067606, reproduced a
readout mismatch during an external native marquee drag. After three native
moves the observer changed the actual `canvas-zoom-percent` QDoubleSpinBox to
200, using its existing valueChanged consumer. Canvas correctly retained zoom
1, but the input field displayed 200% while the actual Canvas readout showed
100%. This persisted after release until the next host refresh; Save already
restored the field to 100% in the original. No Fit action, socket or direct
Canvas zoom call was used by this producer. Original, expected and foreign
native documents and typed History/selection/view oracles were fixed before
Window construction. The native circle remained correctly selected and all
authored state was unchanged.

The Window input callback now reads back Canvas's accepted zoom under the
existing QSignalBlocker after requesting a change. The same frozen probe and
2400 ms / 40-step Windows drag in PID 86020 / HWND 32309858 immediately displayed
100% and retained zoom 1, the expected Circle selection, full committed/preview
Document, revision 0, empty History, Host and independent foreign document.
Exact saved bytes and a distinct Host in the same process matched the full
expected native document. No Canvas policy or authored command changed.

The original focused check failed at the marquee readback assertion. Release
Desktop/tests/probe builds, 14 focused Window checks and registered
`canvas_zoom_readback_contract` 1/1 passed. Focused checks include repeated
refusals, full authored body preview preservation, cancel with existing Undo
and Redo, and accepted ordinary Zoom afterward. Native evidence covers the
marquee and observer-produced actual control signal; it is not physical typing
or native authored-body proof. Driver delivery remains
unknown/global_input/unverifiable; native records and screenshots establish
the outcome. Wheel, DPI/fonts, cold-process reopen, full CTest and whole
Requirement/Mission closure are not claimed. Owned processes closed, Cua
sessions ended and the exact mutex was free/non-abandoned.

RED receipt: `C:/Users/shiro/.codex/scratch/01a141a1-dccb-7023-be1a-c40d6d01e11d/zoom-readback-red.json`,
SHA256 `092FF85918CDD7B4863CBDFA7969AA6FC44B346F6A53B26EE5176AB54292D776`.
Repair receipt: `C:/Users/shiro/.codex/scratch/01a141a1-dccb-7023-be1a-c40d6d01e11d/zoom-readback-checkpoint.json`,
SHA256 `B7517BE8275B1D10D4C3FBD915C8D2AD9CF872F563E653D5D0928153A8E81624`.

## 2026-10-16 native Gradient endpoint on a transformed Object

The unchanged production source at `4879d02` passed an independent native
Gradient endpoint operation; this was acceptance evidence, with no runtime
RED or repair. Before Window construction, the probe fixed complete native-valid
original, expected and foreign documents plus typed Session/History oracles.
The original Session had both Undo and Redo at revision 3. The Object transform
was independently fixed as `world=(320-local_y,240+2*local_x)`.

In native Windows Qt 6.5.3 PID 80192 / HWND 22814544, external Canvas input
selected the Object and actual Tool Rail activation exposed its Gradient.
A 2400 ms / 40-step external drag moved the end handle by screen `(-10,+40)`.
The local endpoint changed exactly from `(40,-20)` to `(60,-10)`, matching the
complete expected committed/preview Document and typed History at revision 4.
During 42 sampled active previews the committed original stayed unchanged.
The stable Object/primitive/operation/Gradient/stop identities, unrelated source,
Host, independent foreign Session, selection, active Rail and zoom 1 were preserved.

Actual Toolbar Undo matched the complete independent undo Session at revision 5;
Redo matched the complete redo Session at revision 6. Exact Host Save bytes and a
distinct Host reopened in the same process matched the full expected document.
The initial Structure UIA Invoke was verified ineffective and earns no selection
credit; actual Canvas input performed selection. Driver delivery remains
unknown/global_input/unverifiable; native records and screenshots prove the
semantic outcome. Owned process closed, Cua session ended, exact mutex free and
not abandoned. Probe build passed; unchanged source checks were not replayed.

This closes only the positive transformed linear Gradient end-handle operation.
Native start/radial/driven/cancellation, foreign active authored replacement,
physical/subjective input, DPI/fonts, chooser, cold-process reopen, full CTest,
Host platform, Requirement and Mission closure remain unclaimed.

Receipt: `C:/Users/shiro/.codex/scratch/01a141bb-1775-7862-925c-35d72a7e2bcf/gradient-native-checkpoint.json`.
SHA256 `D4236C4ACB5B460B44125818078D670D4B07E899C6C994917BC90B91DEABF0F9`.

## 2026-10-16 native primary Gradient Inspector editing

At `86d7849` (metadata-only delta from production source `4879d02`), native
Windows Qt 6.5.3 PID 73424 / HWND 19210040 passed the primary paired End XY
Inspector route without a product repair. Before Window construction, the
probe fixed complete native-valid original/expected/foreign documents and
typed Session/History oracles with both Undo/Redo at revision 3. The expected
transaction was independently fixed as End `(40,-20)` to `(60,-10)`.

Actual Canvas selection and two native scrollbar page clicks exposed the
current Inspector button. The actual paired dialog opened at its exact
unmodified baseline. An unfinished draft was cancelled through the actual
Cancel button, preserving the complete original Session and both Undo/Redo.
After reopening, exact draft `(60,-10)` still preserved the entire original
Document/preview/History until actual Apply committed one revision. Full
expected Document/preview/typed History at revision 4 matched. Actual Toolbar
Undo/Redo matched independent full Sessions at revisions 5/6. Exact Host Save
bytes and a distinct same-process Host reopen matched the full expected source.
Foreign Session, Host, stable source identities, selection and zoom 1 remained
intact. Owned process closed, Cua session ended and exact mutex was free/non-abandoned.

Input delivery was explicitly reconciled before Apply. Background wheel was
refused; a foreground wheel report did not scroll. Actual scrollbar page clicks
worked. UIA text insertion appended and background Ctrl+A did not select;
foreground input also left an incorrect Y draft. No incorrect draft was applied.
Unmodified background Home/Delete cleared Y, and UIA insertion into the empty
field yielded exact `-10`, independently read back with X=`60` before Apply.
These are input-route observations, not a product RED or physical typing proof.
Probe build and diff check passed; unchanged product tests were not replayed.

This proves only the current linear Gradient paired End edit and unfinished-draft
Cancel. Native stale foreign producer, driven/start/radial/input-validation,
physical/subjective/DPI/fonts/chooser/cold-process/fullCTest, Host platform,
Requirement and Mission closure are unclaimed. The preceding endpoint-drag
receipt retains its original `4879d02` binding.

Receipt: `C:/Users/shiro/.codex/scratch/01a141bb-1775-7862-925c-35d72a7e2bcf/gradient-parameter-checkpoint.json`.
SHA256 `D13B964DC95487CA6F75577D272286597EAA7556A561AA2B94CA71CB9E8064D2`.
## 2026-10-16 native transformed Object body Smart Snap

The original production code at `e49762d` (production source `4879d02`, with
metadata changes only) passed an external Windows/Qt 6.5.3 Object body drag.
No product RED or source repair was found. Before Window construction, the
probe fixed complete native-valid original, expected and foreign documents,
plus independent typed Session/History with both Undo and Redo initially
available at revision 3.

The moving rectangle uses `world=(200-local_y,240+2*local_x)` and bounds
X=170..230, Y=160..320. The stationary target has bounds X=280..340,
Y=140..340. Actual Canvas selection followed by a 2400-ms, 40-step external
49-pixel horizontal body drag in PID 85176 / HWND 2950026 snapped by exactly
50 du: `transform.tx` changed from 200 to 250, with `ty=240` unchanged.
The full native Document, pure preview and typed History matched the
independent expected Session at revision 4. All 42 active-preview samples
preserved the committed original and its History; five matched the complete
expected preview. The native screenshot showed both guide lines and
`X: max Object edge → target min · Y: center Artboard → artboard center`.

Actual Toolbar Undo/Redo matched the independent full Sessions at revisions
5/6. Exact Host Save bytes and a distinct Host in the same process matched the
complete expected native Document. Selection, active composition/Artboard,
zoom 1, Host identity, unrelated authored state and owned workspace setting
were preserved. The owned process exited, the Cua session ended and the exact
mutex was free, non-abandoned, released and disposed.

The first probe attempt referenced a nonexistent `tool-object` observer
control and exited before input. Correcting that scratch observer to the real
`tool-selection` control allowed the run above; this was not a product defect.
Driver delivery remains unknown/global_input/unverifiable; native semantic
records and screenshots establish the result. Incidental intermediate
equal-gap feedback is not separate acceptance. No native cancellation,
multi-selection, Direct/pivot, stale/foreign active producer, cold-process,
external-process socket client, physical/subjective, full CTest or combined
REQ-32/33/Mission closure is claimed. Earlier proof keeps its original binding.

Receipt: `C:/Users/shiro/.codex/scratch/01a141d1-cb77-7fa1-a01d-be5349915c3d/body-snap-checkpoint.json`,
SHA256 `F71E2C05656A0EAFD719D926C994897BFEA30A1D5CD25079B3AAF941D2D434D4`.

## 2026-10-16 native Direct Selection on a retained Rectangle generator

The original production code at `ac74200` (metadata-only delta from production
source `4879d02`) passed an external Direct Selection anchor edit without a
product RED or repair. REQ-197 remains Confirmed and partially open. Before
Window construction, a hand-constructed full Point Edit document and a normal
typed Set gesture independently agreed on the complete expected native state
and History. Original/expected/foreign native round-trips were valid; the
initial Session had both Undo and Redo at revision 3.

Actual Canvas selection and the actual Direct Selection Rail control in
Windows/Qt 6.5.3 PID 40708 / HWND 50401848 exposed the stable generated
`moving-source-top-left` anchor. External 2400-ms, 40-step input moved it by
world (+20,+40). Under `world=(200-local_y,240+2*local_x)`, the independent
local result is (-40,-30) to (-20,-50). At revision 4, the complete authored
Document, preview and typed History matched the oracle: the original
Rectangle generator, width 80, height 60 and Object transform remained intact,
with exactly two absolute local overrides in the canonical Point Edit.
The actual Inspector showed the source, enabled Point Edit, two overrides on
one point and x=-20/y=-50. All 42 active-preview samples retained the original
committed Document/History/revision and generator.

Actual Toolbar Undo removed the correction and restored the full original
Document at revision 5; Redo restored the complete expected state at revision
6. Exact Host Save bytes and distinct same-process Host readback matched.
Selection, active composition/Artboard, zoom 1, Host identity, foreign oracle,
unrelated source and owned workspace setting remained intact. The owned
process exited, Cua session ended and the exact mutex was free/non-abandoned.

The driver reports unknown/global_input/unverifiable; semantic native records
and screenshots establish this result. It covers one transformed Rectangle
anchor only. Bypass/reset, source parameter changes, topology changes, other
shapes/handles, cancellation, multi-selection, stale/foreign active producers,
pivot, cold-process, physical/subjective, external-process API, full CTest and
combined REQ-197/Mission acceptance remain separate. Earlier native evidence
retains its original source binding; this proof is not promoted to a later
metadata checkpoint.

Receipt: `C:/Users/shiro/.codex/scratch/01a141d1-cb77-7fa1-a01d-be5349915c3d/direct-point-checkpoint.json`,
SHA256 `A095921077BE6192FBAB9005439094A5D429FA127CA36A168CA5E3DD66B87066`.

## 2026-10-16 native retained Rectangle correction lifecycle

The original production source `4879d02` passed the accepted REQ-197 Rectangle
source/correction lifecycle without a product RED or repair. The actual native
run used HEAD `5b3432c` with only the ownership/plan Brief dirty. Before Window
construction, independently hand-constructed original/Width/bypass/reset/foreign
documents passed full native round-trips and agreed with typed Session commands.
The initial Session had both Undo and Redo at revision 3.

Windows/Qt 6.5.3 PID 87200 / HWND 25040534 used actual Canvas selection, the
visible Width field and Return, visible Point Edit enabled controls, a native
scrollbar page click and the actual Reset confirmation. Width 80 to 120 kept
the saved absolute local correction (-20,-50), full generator/height/transform
and unrelated authored state. Bypass kept both overrides while displaying the
new source corner (-60,-30); re-enable restored (-20,-50). Reset reviewed two
overrides, removed the correction and retained Width 120. Full Document,
preview, native bytes and typed History matched at revisions 4/5/6/7. Actual
Toolbar Undo/Redo matched complete expected Sessions at revisions 8/9.
Selection, composition/Artboard, zoom 1, Host identity, foreign oracle and owned
workspace setting remained intact. Exact Host Save bytes and a distinct
same-process Host reopen matched; the owned process exited, Cua session ended
and the exact mutex was free/non-abandoned.

UIA SetValue initially changed only the Width text, leaving Qt isModified false
and the complete original Session intact. Return did not commit. Background
Home/Delete cleared that draft; ordinary WM_CHAR input produced text 120 and
isModified true, verified before Return. This is an input-route observation,
not a product RED. Driver accessibility/synthetic-events calls report
unverifiable effects; native records and actual screenshots establish outcomes.

This covers one Rectangle with two absolute x/y overrides. Source editing while
bypassed, topology, conversion, other shapes/handles, linked correction,
cancel/stale active producers, cold-process, physical/subjective, full CTest and
whole REQ-197/Mission closure remain separate. Earlier Direct/native proofs
retain their original bindings; none is promoted to this metadata checkpoint.

Receipt: `C:/Users/shiro/.codex/scratch/01a141e6-f117-78e2-aecb-f0243d09bcc4/rect-lifecycle-checkpoint.json`,
SHA256 `0A00A815E0EFCC93E083DFC8F2C068075F2886E924C240D8B1CAECB51DE9A347`.

## 2026-10-16 native Star angular topology and refusal

Original production source `4879d02` passed the accepted REQ-197 topology
correspondence path without product RED or repair. Actual native HEAD `8da2e2d`
had only the next-obligation Brief dirty. Before Window construction, full
independent original/expanded/foreign documents passed native round-trips and
agreed with typed Session/History. The initial revision 3 had both Undo/Redo.
An independent Points 6 command rejected atomically with UNRESOLVED_POINT_EDIT.

Windows/Qt 6.5.3 PID 96180 / HWND 68099100 used actual Canvas selection and the
visible original Star Points field. A verified native text draft 6 and Return
produced QStatusBar currentMessage `UNRESOLVED_POINT_EDIT: moving-source-outer-1-5`.
The complete original Document, native bytes, preview, typed History, revision 3,
both Undo/Redo and view remained intact. The refused draft still displayed 6,
while authored Points remained 10. It was cleared before a verified draft 20.
The later refusal screenshot missed the transient status message; it is not
claimed as visual notification proof. The native observer's actual QStatusBar
record establishes the notification without replaying the valid refusal.

Points 20 committed at revision 4, increasing generated vertices 20 to 40.
The rational angular role `moving-source-outer-1-5` stayed at phase 1/5:
absolute x=245 and generator y=180+60*sin(2*pi/5)=237.06339097770922 persisted.
The external Object's width link to that exact point remained 24.5. Full native
Document/preview/History matched the manual and typed oracles. Actual Toolbar
Undo/Redo matched the complete expected Sessions at revisions 5/6. Exact Host
Save bytes and a distinct same-process Host reopen agreed. Host/view/foreign/
workspace/transform stayed intact; owned process/Cua session ended and exact
mutex was free/non-abandoned.

Driver background accessibility/synthetic-events calls report unverifiable
effects; native records and screenshots establish the reported outcomes.
Linked-only disappearing roles, bypassed topology, conversion, other shapes,
driven point counts, cancel/stale producers, cold-process, physical/subjective,
full CTest and whole REQ-197/Mission closure remain separate. Earlier proofs
keep their original HEAD/owner bindings.

Receipt: `C:/Users/shiro/.codex/scratch/01a141e6-f117-78e2-aecb-f0243d09bcc4/star-topology-checkpoint.json`,
SHA256 `7662E7E6B2CE84380602ED26DF3064C6F798A3C7FF51795D11376C1A9F47C73E`.
