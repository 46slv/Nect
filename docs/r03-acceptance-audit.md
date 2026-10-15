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
