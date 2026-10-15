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
