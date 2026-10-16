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

## 2026-10-16 native explicit conversion and dependency review

The original production source `4879d02` passed this accepted REQ-197 path
without product RED or repair. Actual native HEAD `8a43884` had only the
next-obligation Brief dirty. Before Window construction, full hand-constructed
original/unlinked/converted/foreign documents passed native round-trips and
agreed with typed Session/History. The initial revision 3 had both Undo/Redo.
Independent typed conversion refused atomically with `CONVERSION_REFERENCE`.

Windows/Qt 6.5.3 PID 95660 / HWND 13436228 used actual Canvas selection and
Edit > Convert to Path. The actual review named the external generator
dependency `unrelated / generator.height` -> `moving / generator.width`,
disabled confirmation, and preserved the full original through native Cancel.
An explicit shared Session `Unlink` caller froze that dependent height at 40
at revision 4. This caller is not evidence for a native Unlink control.
Reopening the native review was Session-neutral, showed no blockers and enabled
confirmation. Actual confirmation matched the complete manual converted
Document/native/preview/typed History at revision 5.

Conversion retained the stable contour and all four generated point IDs,
corrected top-left (-20,-50), its active y binding to the external height,
and the incoming external width binding to that point's x (evaluated width 20).
Only the source and Point Edit were removed. Transform, Fill/Stroke/Repeater,
full artwork pixels, Object selection, composition/Artboard, zoom 1, Host,
foreign Document/History and the owned workspace remained intact. Actual
Toolbar Undo/Redo matched full expected Sessions at revisions 6/7. Exact Host
Save bytes and a distinct same-process Host reopen matched. The owned process
exited, Cua session ended and the exact mutex was free/non-abandoned.

The first owned probe referenced nonexistent Rectangle rotation before any
Window. Another observer compared transient selection gesture generation to an
idle oracle; native original bytes/revision were unchanged. Both probe issues
were corrected without product changes before this conversion run. Record 3
of the final run is a legitimate active selection press; settled authored
Session/History are compared fully while the transient epoch 0->1 is recorded
separately. No full-History equality claim covers every active selection sample.
Driver background accessibility/synthetic-event effects are unverifiable;
native records and grounded screenshots establish the reported outcomes.

Bypassed conversion, stale plans, other shapes, physical/subjective operation,
cold-process reopen, full CTest and whole REQ-197/Mission closure remain separate.
Prior Rectangle/Star/native proofs retain their original HEAD/owner bindings.

Receipt: `C:/Users/shiro/.codex/scratch/01a141fe-f62a-75d3-b999-ec8106a3cbff/conversion-checkpoint.json`,
SHA256 `2E25A6FFDA49CF0AC0ECEBF2220CE713D901F26148CDAD77876B2F28FEEE1366`.

## 2026-10-16 native Direct Selection Circle outgoing handle

Original native HEAD `d3495d8dfe12da0d7bdf670f029c44882349543a`
(production `4879d02`, only CURRENT_GOAL metadata dirty at execution) passed
this accepted REQ-197 obligation without a product RED or repair. Receipt:
`C:/Users/shiro/.codex/scratch/01a14216-6573-7e30-b8d3-bf7e8bb33821/direct-handle-checkpoint.json`
SHA256 `5B16E3EF68188CADEC70DB8A81BD52C19A8E38D5669B3EC81990F046C2AB25CE`.

Before Window creation, full hand-constructed original/expected/foreign Documents
round-tripped native encoding; a separate typed Session gesture matched the
manual expected Document and fixed full History, including initial Undo and Redo
at revision 3. The retained Circle has radius 50 and transform
`world=(200-local_y,240+2*local_x)`.

In Windows Qt 6.5.3 PID93712/HWND45876824, actual Canvas selection, actual Direct
Selection Rail, actual east-anchor selection and an external 2400ms/40-step drag
moved the outgoing handle by screen/world `(-10,+60)` and local `(+30,+10)`.
Only `moving-source-east` outgoing angle/length overrides were automatically
added: angle `51.42517627667842`, length `48.11268920014708`. Full Document,
preview, native bytes and typed Session History matched at revision 4. Generator,
all stable point/contour identities, anchor `(50,0)`, incoming handle, other
objects and Object transform stayed intact. Inspector visibly showed the
retained Circle radius and two absolute overrides on one point.

All 43 active samples (41 during the handle gesture, two selection presses)
kept committed original Document/History/revision 3, source, foreign Session,
workspace and zoom 1. Actual Toolbar Undo and Redo matched full typed Sessions at
revisions 5/6. Exact Host Save and a distinct same-process Host reopened the full
expected Document with identical native bytes; live Redo Session was preserved.
The owned process closed, Cua session ended and exact mutex was free,
nonabandoned, released and disposed.

Probe `7370DEB9E395AF323FF05FCEEA913797B7C9E4AC81F791757983899A56C5623D`,
binary `C308ECFD8391A437C21DE0496D292EB8475DB6165A33823AC1B3D7F2984BC794`.
Driver delivery was reported as accessibility/synthetic_events/global_input and
unverifiable; native records and grounded screenshots establish the outcome,
not physical input. Correction reuse, incoming/linked handles, cancellation,
stale/foreign active producers, other shapes, cold-process reopen, full CTest,
physical/subjective/platform and whole Requirement/Mission acceptance remain
separate. Earlier proofs keep their original source/owner bindings.
## 2026-10-16 native incoming handle reuses existing Point Edit and Ref

At actual native HEAD `7fd1fad254b3891ffb00abf9f158c6b1096a65c5`, original
production `4879d02` passed another accepted REQ-197 obligation without repair.
Only CURRENT_GOAL metadata was dirty. Receipt:
`C:/Users/shiro/.codex/scratch/01a14216-6573-7e30-b8d3-bf7e8bb33821/handle-reuse-checkpoint.json`
SHA256 `60D4A298D65D6DDF6B8C6750F54B01F0CDDBE714347F1E1BC63848518C98BBB6`.

Full manual native-valid original/expected/foreign Documents and separate typed
Session/History were fixed before Window creation. The original Circle has
radius 50, transform `world=(200-local_y,240+2*local_x)`, existing east outgoing
angle 15/length 35 overrides, and a second Object's generator.width linked to
the east incoming handle length. Initial revision 3 has both Undo and Redo.

Actual Windows Qt6.5.3 PID90880/HWND3414666 Canvas/Direct Rail/east-anchor
selection and external 2400ms/40-step incoming-handle drag by world `(+10,-60)`
(local `(-30,-10)`) matched full expected Document/preview/native bytes/typed
History at revision 4. The same Point Edit ID `moving-source-point-edit` now
contains four overrides: existing outgoing angle/length remain exactly 15/35;
incoming angle/length become -128.57482372332157/48.11268920014708. The stable
incoming-length Ref stays authored, and its external width evaluates to the
same new length. Source, anchor `(50,0)`, other points/objects and transform
remain as manually specified. Inspector shows four overrides on one point.

All 40 observed active samples (39 during handle drag, one selection press)
preserve committed original Document/History/revision 3, source, foreign
Session, workspace and zoom 1. Actual Toolbar Undo/Redo match full typed
Sessions at revisions 5/6, including external width restoration. Exact Host
Save/distinct same-process Host matches full expected native bytes and leaves
the live Redo Session intact. Owned process closed, Cua session ended and exact
mutex was free, nonabandoned, released and disposed.

Probe `0059D0B53E79BA83FE7B1D8780980EED1C6AA78D420BFA149BC845ED09FFEDD1`,
binary `51F654BA8837C7D2AB96A20ADCA1BB4C88FF455D6BF1A80066AC1138911F1B0E`.
Driver accessibility/synthetic_events/global_input reports are unverifiable;
native records and grounded screenshots prove outcomes, not physical input.
Driven edited fields, cancel/stale/foreign active producers, other shapes,
cold-process reopen, full CTest and whole Requirement/Mission remain separate.
Previous proof keeps its original HEAD, not this later documentation checkpoint.
## 2026-10-16 native mixed-source multi-point Direct Selection

Original production `4879d02` passed this R03 / REQ-197 / REQ-210 subset
without a product RED or repair. Actual native HEAD was
`16d4a0c4462cb639246557f42796e39b3d96c34e`, with only Brief metadata dirty.
Receipt: `C:/Users/shiro/.codex/scratch/01a14227-31a4-7a63-87da-87b290b24159/mixed-direct-checkpoint.json`,
SHA256 `6A580007F87D8241AA88BE66905FFA8F5C6A5D06919EED42F83C1C7E30636F15`.

Before Window construction, full independent original, expected and foreign
Documents passed native round-trips and typed Session/History oracles were
fixed. Original revision 3 had both Undo and Redo. Windows Qt 6.5.3
PID75240/HWND75108752 used owned INI/FolderLibrary/recovery paths;
probe SHA78CF5C77 and binary SHA6C4F95AA.

Actual Direct Rail and an empty-space native marquee selected curve-first and
Circle source-east. External drag moved both anchors by world (+20,+10).
Circle's world=(180-local_y,120+2*local_x) yields local (55,-20) from (50,0).
Curve's world=(360+2*local_x,260+local_y) yields local (10,10) from (0,0).
Full Document/native/preview/History matched revision 4. Only Circle gained
its stable downstream PointEdit; source, other anchors/handles/transforms
remained intact. Unselected consumer kept both stable references and
evaluated width55/height210.

All 33 active samples retained committed original Document/History/revision 3
and both Undo/Redo. Actual Toolbar Undo/Redo matched full typed Sessions at
revisions 5/6. Exact Save bytes and distinct same-process Host reopen matched
the full expected Document. Selection, Host, composition, Artboard, zoom 1,
foreign Session and owned workspace were preserved. Owned process closed,
both Cua lifecycles ended, exact mutex free/non-abandoned/released/disposed.

The first marquee start was inside Circle and selected its whole Object;
corrected empty-space start selected exactly two intended anchors without
authoring. First CLI helper refused missing screenshot context before input;
CLI-owned capture fixed that prerequisite. Queued Escape reached Canvas only
after native release/commit and exited idle Direct mode. This does **not**
establish native active cancellation. That exact row is LOCAL_WAIT until
supported held-pointer/key interleaving is available; do not repeat the
serialized attempt or call it a product failure. Original typed/Qt cancel
evidence retains its own bindings.

Cua reported accessibility/synthetic_events/global_input with unverifiable
effects. Native records and grounded screenshots establish outcomes.
Physical input, subjective quality, stale/foreign active producers, other
multi-point transforms, cold-process reopen, full CTest and whole Requirement
or Mission closure remain unproven. Older receipts retain original
owners/HEADs and are not promoted to this checkpoint.

## 2026-10-16 native Anchor Edit on a transformed retained Circle

Original production `4879d02` passed this R03 / Tool Rail Anchor operation
without a product RED or repair. Actual native HEAD was
`d3dd5c5b646ee1b661fe21b8eac6f77375fc21e1`, with only Brief metadata dirty.
Receipt: `C:/Users/shiro/.codex/scratch/01a14227-31a4-7a63-87da-87b290b24159/anchor-checkpoint.json`,
SHA256 `48260AA366E30840A226DBD6309FECDEB1C406C3D30CE5123330AE2E6E1124F0`.

Before Window construction, full independent original/expected/foreign Documents
passed native round-trips, with typed Session/History and pixel oracles fixed.
Original revision 3 had both Undo and Redo. Expected Anchor-only Document
preserved the selected Circle's entire rendered image; full expected scene
also included the intentional linked consumer width change.

Actual Canvas selection and Anchor Rail activation in Windows Qt 6.5.3
PID91748/HWND59841714 preceded the external 2400ms/40-step pivot drag.
World (+20,+20) under world=(200-local_y,240+2*local_x) changed authored Anchor
from local (0,0) to (10,-20). Full Document/native/preview/History matched
revision 4. Generator radius50, existing stable PointEdit outgoing15/35,
all path/paint/source data and affine matrix stayed intact. The unselected
consumer retained its anchor_x reference and evaluated width110.

All 22 active samples retained committed original Document/History/revision 3
and both Undo/Redo; all observed selected-Circle images remained pixel equal.
Actual Toolbar Undo/Redo matched full typed Sessions at revisions 5/6.
Exact Save bytes, distinct same-process Host reopen and full expected scene
pixels matched. Selection, active Anchor mode/Rail, Host/view/foreign Session
and owned INI/FolderLibrary were preserved. Owned process closed, Cua ended,
exact mutex free/non-abandoned/released/disposed. Probe SHA238FB93E,
binary SHAB3F7AEFA; receipt binds 25 artifacts.

Cua reported accessibility/synthetic_events/global_input with unverifiable
effects; semantic records and grounded screenshots establish outcomes.
Native cancellation, driven edited Anchor, foreign/stale active producers,
other transforms, physical/subjective input, cold-process reopen, full CTest
and whole Requirement/Mission closure remain outside this proof.

The separately considered active socket Open producer was rejected before
native execution: current Host::reset (src/desktop/host.cpp:168) refuses an
active Session gesture with GESTURE_ACTIVE before replacement. The older
native socket marquee proof had no authored Session gesture and is distinct.
No source bypass, manufactured RED or duplicate native qualification was used.
Source reconciliation is in OWN/authored-producer-reconciliation.json.
## 2026-10-16 native transformed radial Gradient centre and radius

Original production `4879d02` at actual native HEAD `42aa49f` passed this
independent R03 / REQ-210 Gradient Edit operation. Only Brief metadata was dirty
at the producer; no product RED or repair occurred. Before Window construction,
manually authored full original/centre/final/foreign native documents, complete
typed Session/History and distinct artwork pixel oracles were fixed and agreed.
The initial Session had both Undo and Redo at revision 3.

Windows Qt 6.5.3 PID 79776 / HWND 24580488 used actual Canvas selection and
Gradient Rail activation. Under `world=(320-local_y,240+2*local_x)`, an external
2400 ms / 40-step centre drag by world `(10,40)` moved local `(-30,-10)` to
`(-10,-20)` at revision 4. A second actual radius-handle drag by world `(-15,40)`
moved local `(40,-10)` to `(60,5)` at revision 5. Both matched complete native,
preview and typed History oracles. Stable external Gradient Refs changed the
consumer width 70 to 90 and height 80 to 110. Unrelated source, all IDs, Host,
foreign Session, active Rail, selected Object and workspace were preserved.
During 41 centre and 40 radius active observations, the committed starting
Document and History of each gesture stayed unchanged. The pre-ready layout
record had zoom 0.02; all records from ready onward, including native input,
had zoom 1.

Actual Toolbar Undo twice / Redo twice matched full typed Sessions at revisions
6/7/8/9. Exact Save bytes, a distinct Host reopened in the same process and
complete expected artwork pixels matched. Process closed, Cua session ended,
exact mutex was free, non-abandoned and released/disposed. The owned probe's
const-pointer initializer correction and refused Cua capture/token arguments
preceded effective input and were preparation issues, not product REDs.
Driver synthetic/global-input effects remain unverifiable; native records and
grounded screenshots establish outcomes without claiming physical input.

This closes only these transformed radial centre/radius operations. Driven
edited axes, native active cancellation, other transforms, cold-process reopen,
physical/subjective/DPI/fonts, full CTest, Host platform and whole Requirement or
Mission closure remain unclaimed. Existing evidence retains its original HEAD.
Receipt: `C:/Users/shiro/.codex/scratch/01a14240-6bb0-72d3-8f89-b3fa5bd04b93/radial-checkpoint.json`.
SHA256 `8D0690CDBAE5A2D1E241C75905B196F352B007A66CEE5568FB37095D6F3E4432`.
## 2026-10-16 native Gradient free-axis edit with a driven axis

Original production `4879d02` at actual native HEAD `58f9bcd` passed this
independent R03 / REQ-210 boundary without product RED or repair. Only Brief
metadata was dirty. Full manually authored original/expected/foreign native
documents, distinct artwork pixels, typed Session/History and a complete typed
`DRIVEN_PROPERTY` refusal oracle were established before Window construction.
The initial revision 3 Session had both Undo and Redo.

Windows Qt 6.5.3 PID 38040 / HWND 18421730 used actual Canvas selection and
Gradient Rail activation. Gradient centre X remained linked to the unrelated
Circle radius by a stable Ref, evaluating to -30 with its authored literal and
binding preserved. Under `world=(320-local_y,240+2*local_x)`, an external native
2400 ms / 40-step drag by world `(20,0)` changed only free centre Y from -10 to
-30. Complete native/preview/typed History matched revision 4, and the external
free-Y Ref consumer width changed 90 to 70. During 22 active observations,
committed original and History stayed unchanged.

A second actual native diagonal drag by world `(10,40)` attempted to change
driven X as well as Y. Visible `DRIVEN_PROPERTY` feedback accompanied full
Document, preview, History and revision 4 preservation with no active gesture.
Ten active observations retained the committed expected document and History;
any transient free-Y preview was canceled atomically. X binding/literal/value,
other Gradient/source fields, all IDs, Host, foreign Session, selection, active
Tool and workspace remained intact.

Actual Toolbar Undo/Redo matched complete typed Sessions at revisions 5/6.
Exact Save bytes, a distinct same-process Host and full expected artwork pixels
matched. Owned process closed, Cua session ended, exact mutex free/non-abandoned
and released/disposed. Driver accessibility/synthetic/global-input effects are
unverifiable; native records and grounded screenshots prove semantic outcomes,
not physical input. Other driven axes/source expressions/transforms, native
active cancellation, physical/subjective/DPI/fonts, cold-process reopen, full
CTest, Host platform and whole Requirement/Mission remain separate.

Receipt: `C:/Users/shiro/.codex/scratch/01a14240-6bb0-72d3-8f89-b3fa5bd04b93/driven-checkpoint.json`.
SHA256 `BE76FA677D7004C872774A7433CA343DCED55A9A34EE5C330BFC274A23DD9AC9`.

## 2026-10-16 native source edit while Point Edit is bypassed

REQ-197 requires generator editing to continue while a correction is disabled,
and topology changes to report lost correspondence. The original production
source passed this previously open path; no product repair was needed.

The owned Windows/Qt 6.5.3 Window ran at actual HEAD
`16fbe4709cd266d9aee04b44559685294e396800`, production
`4879d02f1849bba810d1870736ea055693db923b`, with only CURRENT_GOAL.md dirty.
Before constructing the Window, the probe fixed complete manual native-valid
original, bypassed, radius-edited, re-enabled and foreign documents, independent
typed Session/History, trigonometric angular coordinates, external Ref widths,
full raster oracles and fixed red/black/white samples. All authored transitions
had distinct pixels. The initial revision 3 had both Undo and Redo available.

PID 17352 / HWND 23732364 accepted an actual Canvas selection and Properties
Point Edit toggle. Bypass committed revision 4 while retaining the same correction
ID and absolute X=245 override. Actual key input changed outer radius 60 to 90
while bypassed, committing revision 5. The visible angular point followed the
source to X=207.81152949374527; its external Ref consumer width became
20.78115294937453. An actual Points=6 submission displayed
`UNRESOLVED_POINT_EDIT: moving-source-outer-1-5`, retaining points=10 and the
entire committed/preview Document, native bytes, History and revision 5.
Re-enabling the correction committed revision 6, retained radius 90 and restored
the absolute X=245 and linked width 24.5.

Actual Toolbar Undo three times and Redo three times matched all six complete
expected Sessions and raster oracles at revisions 7 through 12. All 25 idle
records matched full native/preview/History/revision/Undo availability and pixels;
source identity, transforms, selection, zoom, foreign Session, Host and owned
workspace marker were preserved. One active record belonged to the neutral
Canvas selection click and was excluded from full-stage comparisons; it is not
an active-gesture purity proof. Exact saved bytes and a distinct Host in the
same process reopened the full expected document and pixels without changing
the live final History. Both owned preparation/runtime processes exited, the
Cua session ended, and the exact build mutex was free and non-abandoned.

UIA text insertion and posted modifier delivery initially changed only the draft
at revision 4. Readback reconciled that state before real key input committed
the intended value. An ineffective scroll was likewise reconciled before using
the actual accessibility scrollbar. The first owned fixture lacked consumer
paint and stopped before Window construction; adding its explicit black Fill
made the independent pixel samples valid. These were preparation/input-route
corrections, not product failures. Driver reports include accessibility,
synthetic events and global input with unknown/unverifiable effect; native
records and grounded screenshots establish outcomes, not physical input.

Receipt:
`C:/Users/shiro/.codex/scratch/01a1425b-3073-7e93-9a03-00d30eb4f768/bypassed-checkpoint.json`,
SHA256 `F27198589AF81275CCFDABD430A34B782BB79164971D69BFE5E1DB1AB4FAF2F6`,
binds 79 artifacts. The exact Star literal bypass/radius/dormant corrected-role
path is closed. Linked-only disappearing roles, other source inputs/shapes,
conversion, active cancellation, stale/foreign producers, cold-process/socket,
full CTest, physical/subjective and Requirement/Mission closure remain unclaimed.
Earlier native proofs retain their original owner/source bindings.

## 2026-10-16 native linked-only angular role and source editing

An independent original Star fixture had no Point Edit at all. An unrelated
Rectangle width referenced `moving-source-outer-1-5/x`, so only an external Ref
protected that rational angular role. REQ-197's topology correspondence boundary
was exercised separately from the corrected/dormant-override guard above.

Complete manual native-valid original, expanded, radius-edited and foreign
documents, typed Session/History, trigonometric coordinates, external Ref values,
full raster oracles and fixed pixel samples existed before Window construction.
Native actual HEAD was `71de640407cd27605d21293a126366ee343c188a`, unchanged
production `4879d02f1849bba810d1870736ea055693db923b`, with only CURRENT_GOAL.md
dirty. The original Session at revision 3 had both Undo and Redo.

PID 93144 / HWND 17113286, Windows/Qt 6.5.3, accepted actual Canvas selection
and real Properties key submissions. Points=6 displayed
`MISSING_REFERENCE: moving/moving-source-outer-1-5/x` and preserved the complete
native/preview/History/pixels/revision 3 and points=10. Points=20 committed
revision 4 with 40 vertices, retaining the same role and binding; linked width
remained 19.854101966249686. Outer radius 60 to 90 then committed revision 5,
moving that point X to 207.81152949374527 and linked width to
20.78115294937453. No Point Edit was created.

Actual Toolbar Undo twice and Redo twice matched complete expected Sessions and
pixels at revisions 6 through 9. All 16 idle native records matched all seven
expected stages, with source IDs, external binding, transforms, selection, zoom,
foreign Session, Host and workspace marker preserved. Exact saved bytes and a
distinct Host in the same process reopened the full expected document and pixels
while retaining the live final History. The owned process closed, Cua ended,
and the exact mutex was free/non-abandoned/released/disposed.

Receipt:
`C:/Users/shiro/.codex/scratch/01a1425b-3073-7e93-9a03-00d30eb4f768/linked-checkpoint.json`,
SHA256 `163BE914AABDE3177118668AFF411629983B3197035246CA68EA2ECBD64A7A87`,
binds 51 artifacts, including pre-native source identity and full independent
oracles. Probe AA8920D2 / binary 1D52AF0C. A final-stage pixel-map label in the
owned observer was corrected before Window construction; product code was not
changed. Driver delivery remained synthetic/accessibility/global input and
unverifiable; native records and grounded screenshots establish the outcomes.

This exact linked-only Star role/refusal/accepted topology/radius path passed
without product RED or repair. Other shapes/expressions, conversion, stale plans,
native active cancellation, foreign active producers, cold-process/socket/full
CTest/Host-platform and Requirement/Mission completion remain unclaimed.
All earlier proofs retain their original source and owner identities.
## 2026-10-16 native conversion with bypassed Point Edit

Original native HEAD `03199cb5e8ebc741209c63f9e55ba367b52b4d41`
(production `4879d02`, only CURRENT_GOAL metadata dirty) passed this Confirmed
REQ-197 subset without product RED or repair. Full manual original, converted
and foreign Documents passed native round-trips before Window construction.
Typed Session/History, initial revision 3 with both Undo/Redo, fixed source
coordinates and full artwork pixel equality were established before input.

Windows/Qt 6.5.3 PID62628/HWND42933828 used actual Canvas selection and the
Properties Convert to Path control. The actual review warned that bypassed
corrections would be discarded. Native Cancel preserved the full original,
including disabled Point Edit, stored x=-20 and dormant y driver to the external
height. Reopening remained neutral; actual confirmation at revision 4 matched
the full manually constructed converted Document and typed History.

Conversion used the source top-left (-40,-30), removed the generator and dormant
correction/driver, and preserved the contour/four point IDs, incoming external
point Ref (consumer width40), transform and later Fill/Stroke/Repeater. Full
artwork pixels stayed identical. Actual Toolbar Undo at revision 5 restored all
source/correction/driver state; Redo at revision 6 matched the full expected
Session again. All nine idle records and all four stages matched; one active
Canvas selection press was excluded from full-stage comparison. Gesture epoch
was recorded separately, with no claim of full active-gesture purity.

Exact Host Save bytes and a distinct same-process Host reopen matched. Host,
foreign Document/History, selection, composition/Artboard, zoom1 and the owned
workspace were preserved. Process closed, Cua session ended, exact mutex was
free/non-abandoned. Driver routes were background accessibility/synthetic
events with effect unverifiable; native records and grounded screenshots prove
outcomes, not physical input. Own preparation stopped on an absent whitespace
fragment before probe output/Window; only its script was corrected.

Stale plans, other shapes, physical/subjective input, cold-process reopen,
full CTest and whole Requirement/Mission closure remain separate. Earlier
conversion evidence retains its original producer identity and acceptance limits.

Receipt: `C:/Users/shiro/.codex/scratch/01a14279-b0e3-7e70-9c7c-f8a5073a2ba2/bypassed-conversion-checkpoint.json`,
SHA256 `6550151433DD03B03152F85BCE37674FF7B725A8FDC279602404975955AA9639`.

## 2026-10-16 native stale conversion review after a source change

Original native HEAD `53c542b3e463730a1f03beec798ff8d5f84c8012`
(production `4879d02`, only CURRENT_GOAL dirty) passed this REQ-197 review
context boundary without product RED or repair. Full manual original, changed,
converted and foreign Documents, native round-trips, typed Session/History and
stage-specific pixel oracles were fixed before the Windows producer. Initial
revision 3 had both Undo/Redo; changed-source pixels differ from original and
equal independently converted pixels.

Windows/Qt 6.5.3 PID95308/HWND51710724 used actual Canvas selection and native
Properties conversion review at source width80. An explicit canonical typed
Session Set changed width to100 at revision4 while the plan remained open.
This caller is not evidence for native width editing or an external-process API.
Actual stale confirmation displayed REVISION_CONFLICT and preserved the full
changed Document/native/preview/History/pixels. Native Cancel remained neutral.

Reopening created a fresh native plan with no error; actual confirmation at
revision5 used source top-left(-50,-30) and external point consumer width50,
matching the full manual converted Document and typed History. It did not use
old coordinates(-40,-30)/consumer40. Source and bypassed correction were removed;
stable contour/four point IDs, incoming point Ref, transform and the later
Fill/Stroke/Repeater remained. Actual Toolbar Undo6 restored the changed-width
source and dormant correction/driver; Redo7 matched the complete expected Session.
All five stages and eleven idle records matched. One active selection press
was excluded from full-stage equality, with gesture epoch recorded separately.

Exact Host Save and distinct same-process Host reopen matched. Host, foreign
Document/History, selection, composition/Artboard, zoom1, workspace and all
stage-specific artwork pixels were preserved. Process closed, Cua session ended,
exact mutex free/non-abandoned. Background accessibility/synthetic events
reported effect unverifiable; native records and grounded screenshots establish
outcomes without a physical input claim. No guard bypass or manufactured foreign
active producer was used.

Other stale Document/Host/gesture contexts, other shapes, physical/subjective,
cold-process/external-process API, full CTest and whole Requirement/Mission
closure remain unclaimed. Earlier native proofs retain original source identities.

Receipt: `C:/Users/shiro/.codex/scratch/01a14279-b0e3-7e70-9c7c-f8a5073a2ba2/stale-conversion-checkpoint.json`,
SHA256 `6612473F3281BED79211A7C16547D33D214CBFCFA604E8C7804D8429082FE65B`.

## 2026-10-16 native Gradient HEX preserves unedited precision

Original production `4879d02`, at actual native HEAD `6773088b` with only
CURRENT_GOAL dirty, passed the existing Gradient Inspector HEX route without
product RED or repair. Full manual native-valid intended/original/foreign
Documents, typed Session/History and full stage pixel oracles existed before
Window construction. Initial revision 3 had both Undo and Redo.

Windows Qt 6.5.3 PID93052/HWND12846882 used external Canvas selection and three
actual Properties scrollbar page clicks to reach Stop 1. The intended edit was
`#1F3C58E0` to `#993C58E0`, changing only red to Qt's parsed value
`0.6000000238418579`. Green `0.234567890123456`, blue `0.345678901234567`
and alpha `0.876543210987654` retained their exact authored values. A stable
same-Document color Ref updated another Fill red to `0.5750000178813934`;
all source/operation/Gradient/stop IDs, other paint/frame and contour were kept.

UIA SetValue alone changed text with modified=false and left the whole original
Session intact. Background native End/Left/Delete/0 restored the intended text
with modified=true; full original Document/preview/History/pixels still matched
before Return. Actual Return committed once at revision 4. Actual Toolbar Undo5
and Redo6 matched full independent Sessions and stage pixels. Exact saved bytes
and a distinct same-process Host readback matched the intended full Document.
Foreign Session/History, Host identity, selection, zoom1 and owned workspace
were preserved. All14 idle records matched; one active Canvas selection record
was excluded from idle comparison, with no active-purity claim.

Preparation first refused an incompatible color-to-length fixture before Window;
the consumer was corrected to a same-unit Fill color. A second preparation had
an incorrect DF HEX annotation; its original Session stayed intact and closed
before editing. The final producer uses E0; full manual expected Document was
unchanged by that annotation correction. Earlier preparation is not acceptance.
Process closed normally, Cua ended, exact mutex free/non-abandoned. Background
accessibility/synthetic events reported unverifiable; native records and grounded
screenshots prove outcomes, not physical input. Probe build/diff check passed;
unchanged product tests were not replayed.

Six-digit implicit alpha, invalid input, other channels/drivers/native stale or
foreign producers, cold-process/DPI/physical/subjective/fullCTest and whole
Requirement/Mission closure remain unclaimed. Prior native receipts retain
their original source identities.

Receipt: `C:/Users/shiro/.codex/scratch/01a14293-714a-7700-9bf9-16788e7ec439/gradient-hex-checkpoint.json`,
SHA256 `3165297C86A001D429A4680FA951EA7F60BF81EADDB45B83B4CE218CF64EAC12`.

## 2026-10-16 native six-digit HEX explicitly makes alpha opaque

At actual native HEAD `2cb1911b` (production `4879d02`, only Brief dirty), the
original Gradient Inspector passed six-digit opaque-alpha intent without a
product RED or repair. This differs from preserving unedited eight-digit color
channels: the authored alpha `0.999999999999` already displays `FF`, but entering
six RGB digits explicitly requests exact alpha `1`.

Full manual intended/original/foreign native-valid Documents, typed
Session/History and full stage pixel oracles preceded Window. RGB literals were
`0.123456789012345`, `0.234567890123456`, `0.345678901234567`; only intended
alpha changed to 1. A stable Ref from another Fill alpha used scale .75/offset
.125. Original and expected pixels were independently equal before Window,
while full authored Documents/native bytes differed. Initial revision 3 had
both Undo and Redo.

Windows Qt6.5.3 PID94872/HWND34278110 used actual Canvas selection/Properties
scrolling, then background native End/Left/Left/Delete/Delete removed displayed
`FF`, producing the exact six-digit draft `#1F3C58`. Full original Session and
pixels stayed intact before Return. Actual Return committed once at revision 4,
setting alpha to 1 and consumer alpha to .875 while all RGB precision and IDs,
source/other stop/frame/paint/contour stayed intact. Actual Toolbar Undo5 restored
alpha .999999999999/consumer .87499999999925 and Redo6 restored exact opacity.
All12 idle records matched independent full Sessions and pixels; one active
selection record was excluded, without an active-purity claim.

Exact Host Save bytes and distinct same-process Host reopen matched. Foreign
Session/History, Host identity, selection, zoom1 and owned workspace persisted.
Process closed normally, Cua ended, exact mutex free/non-abandoned. All input
was background accessibility/synthetic events with effect unverifiable; native
records and grounded screenshots prove the outcome, not physical input. Probe
build/diff check passed; unchanged product tests were not replayed.

Other channels/drivers, invalid input, stale/foreign active producers,
cold-process/DPI/physical/subjective/fullCTest and whole Requirement/Mission
closure remain unclaimed. The preceding eight-digit proof keeps actual6773088b.

Receipt: `C:/Users/shiro/.codex/scratch/01a14293-714a-7700-9bf9-16788e7ec439/gradient-alpha-checkpoint.json`,
SHA256 `83597933D2F4858DCF86A49F4953F34B4737CDF317424C6E5F83ADFDDB2EB0EF`.

## 2026-10-16 native Object opacity expression entry

Original native HEAD `71ed02a800d060c6721ac4f6fd6087a2e1c5d3fa`, production
`4879d02`, passed this R02/R03 / Confirmed REQ-42 subset without product RED or
repair. Only Brief metadata was dirty during production input. Windows Qt6.5.3,
PID95244/HWND22088434; source B972D7B8, executable 0AB66B5A.

Before Window construction, independent complete original/expected/foreign native
documents round-tripped, manual intended Document matched a typed Session command,
and complete History and artwork-pixel stages were fixed. Initial revision3 had
both Undo and Redo. Actual Canvas selection and Inspector Object opacity draft
followed by native Return applied `ref("source","","composite.opacity") * 0.5`
at revision4. Evaluation became .35 and a stable Ref consumer became .275, while
the authored literal .812345678901234 and every unrelated field stayed intact.
Actual Toolbar Undo5 and Redo6 matched the full typed History/native/pixel oracles.
All13 authored records matched;12 post-ready records retained zoom1. The first
pre-ready zoom.02 record also matched the original full authored state and is
excluded only from the post-ready view claim. The selection generation was
independently fixed before Window:0 before selection and1 afterwards.

Preparation run01 closed with the original intact before Return: Cua type_text
had appended to existing numeric text. Final run02 used UIA SetValue, then native
End/Left/Delete to remove one trailing space and establish a modified draft.
The entire original Session remained intact until Return. Expected Document and
History were never adjusted after production input. A preliminary summary query
over-applied settled zoom1 to startup; its correction classified that startup
record without changing source/oracles or replaying actions.

Exact Host Save bytes and distinct same-process Host reopen matched, including
full artwork pixels. Host identity, foreign Session/History and owned workspace
persisted. Process closed normally, Cua ended, exact mutex free/non-abandoned.
Background accessibility/synthetic events and grounded screenshots establish
outcomes; physical input, invalid expressions, stale/foreign incoming producers,
active gestures, transport parity, cold/external process and whole Requirement
or Mission closure remain unclaimed. Unchanged product tests were not replayed.

Receipt: `C:/Users/shiro/.codex/scratch/01a142b4-81a8-7970-8054-6b128ade5c1c/opacity-expression-checkpoint.json`,
SHA256 `A00B2E3FFABC77BB83A82A32D7A7EC55A9484A13687A65F04B07DB0C74E8AC06`.

## 2026-10-16 native driven opacity refusal and invalid expression draft

Original native `229605f18026d3c77327511a9709631d7d17626b`, unchanged production
`4879d02`, passed this distinct REQ-42 negative editing lifecycle. Only Brief
metadata was dirty. Windows Qt6.5.3 PID88436/HWND5704738; source C716D123,
executable D4C3599F. No product RED or repair.

Complete original/expected/foreign native, typed History and pixel oracles were
fixed before Window. The original already retained an opacity expression and a
downstream Ref, with revision3 and both Undo/Redo. Native numeric .4 Return was
refused with DRIVEN_PROPERTY. Native `=ref("source","","composite.opacity") / 0`
Return expanded an uncommitted inline draft with a division-by-zero message.
Explicit native Apply also refused; native Cancel closed the draft and displayed
the committed .35. All15 idle records preserved the full original Document,
native, typed History, .812345678901234 literal, original expression and .275
consumer, Host/foreign/workspace, and artwork pixels. One active selection record
is retained but excluded from idle-preview acceptance; no active purity claim.
Fourteen post-ready idle records retained zoom1; startup is separate.

Exact Save and distinct same-process Host reopen matched the unchanged native
and full pixels. Process closed, Cua ended, exact mutex free/non-abandoned.
All input was background accessibility/synthetic events. Physical input,
stale/foreign incoming producers, source replacement, active gestures,
cold/external process, transport parity, full CTest and whole Requirement/Mission
closure remain unclaimed. Unchanged product tests were not replayed.

Receipt: `C:/Users/shiro/.codex/scratch/01a142b4-81a8-7970-8054-6b128ade5c1c/opacity-refusal-checkpoint.json`,
SHA256 `730F4695BC99F74D77212F6421C128B60B83018B7D08FF46D3ADAD380769DBC4`.

## 2026-10-16 native Pen creation, closure and independent Path continuation

Original native `9cce3ddbd602ff96e32c369dee8b134af1caca16`, unchanged production
`4879d02`, passed this bounded R03 / Confirmed REQ-210 authoring lifecycle.
Only Brief metadata was dirty. Windows Qt6.5.3 PID88256/HWND46865956;
source FF258E49, executable E7F4EE48. No product RED or repair.

Before Window, the probe fixed complete manual native Documents, typed Session
History and artwork pixel oracles for revisions3..14, with both initial Undo/Redo.
Only generated UUID identities were bijectively mapped to predeclared roles of
identical length; coordinates, source, topology, paint, order and full History
were fixed independently. Native Pen activation retained revision3. Canvas
points (170,130), (310,150), (250,290) each committed one canonical edit; first
anchor click closed the contour at revision7. Actual Toolbar Undo8/Redo9 restored
the entire open/closed source and History. The next point created a distinct Path
at (390,170), followed by (470,250). Native Return finished that open Path without
an authored edit; the next point (400,330) created a third distinct Path. Toolbar
Undo13/Redo14 preserved all original IDs and unrelated source. Native Escape
selected Selection without changing revision14 or full authored state.

All15 records matched the complete manual native, typed History, preview and
stage pixels, with zero gesture generation; all14 post-ready records retained
zoom1. Host identity, independent foreign Session/History and owned workspace
were preserved. Exact raw saved/live native bytes, distinct same-process Host
reopen and full reopened pixels matched. Process closed, Cua ended, and exact
mutex was free/non-abandoned/released/disposed.

Preparation is retained separately: the first run stopped before Window because
the manual new-Path anchor omitted first-point initialization. After correcting
that fixture, run02's first native click landed Y131 instead of intended Y130
because screenshot/input origin differed by one pixel; it stopped after that
single point. Run03 used screenshot-grounded input Y-1. Its entire fixed native
and History oracle remained byte-identical to run02; no expected geometry was
adjusted after production. These are preparation failures, not product REDs.

Input used background accessibility/synthetic events, mostly reported as
unverifiable; grounded screenshots and semantic records prove outcomes, not
physical input. Held-pointer cancellation, stale/foreign incoming producers,
other drawing scopes, cold/external process, full transport/CTest and whole
Requirement/Mission closure remain unclaimed. Valid product tests were not replayed.

Receipt: `C:/Users/shiro/.codex/scratch/01a142d2-df6b-7d61-a48a-5745f209af28/pen-checkpoint.json`,
SHA256 `8036129694321A4C0302A826953F637EF4F56811EBD378A9987460C74906705B`.
