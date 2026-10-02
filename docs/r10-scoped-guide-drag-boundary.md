# R10 scoped Artboard/Template Guide drag / bounded first vertical

Status: QUALIFIED bounded cloud candidate / remote synchronization pending. Base 07949e0dc567e0b94f3dff5b26f59969661fe515.
Owner: dot, one cloud writer. Existing REQ-205/G5 and Guide drag residual apply.

## Interaction decision

Use the existing Artboard Guide occurrence selector plus an explicit one-shot
Drag action. It arms exactly one stable target occurrence in the active Artboard.
Only that visible clipped segment is a Guide hit target while armed, even if a
global or another scoped Guide overlaps. No arbitrary active/lexical precedence
among unselected occurrences is introduced. Without an armed occurrence, preserve
legacy Composition-global nearest/ID-tie Guide hit behavior.

One shot ends/disarms on release, Escape, mode-off, hide, active-Artboard change or
stale context. A miss does not pick a different Guide. Existing ordinary selection
behavior may continue after the one-shot is canceled. This is an explicit access
path, not a claim of general click-any-scoped-Guide UX. Hidden, disabled, suppressed
and outside-frame occurrences cannot be armed/hit. Do not silently reveal overlays.

## Mutation contract

Capture document/session/revision, Composition, target Artboard, authored source
Artboard, stable Guide ID and inherited/local kind. Freeze inverse view and local
starting position on press. Pointer-axis delta through that view updates local
position; frame origin is only for hit/drawing, never persisted as local position.
Authored local -> UpdateArtboardGuide preserving fields; inherited -> target-only
position override. Never edit the parent from an inherited occurrence or detach.

Reuse Session gesture previews, one release/Undo and cancel semantics. Returning
to the starting coordinate uses empty-command preview reset, preserving an absent
or pre-existing override exactly. Invalid preview cannot commit an earlier valid
preview on invalid release. Any revision/session/source/context change cancels;
no rebase or retarget. A valid drag beyond frame bounds retains its coordinate and
may disappear, consistent with the existing authored Guide contract.

## Acceptance

S/GX40; A origin1000 override90; B origin2000 inherit40. A+20 ->110, B+20 ->60,
source unchanged; Undo B removes its new override. Local/authored source edits
use their own occurrence. Cover both axes, nonunit zoom, negative/nonzero origins,
frozen view during Fit, same-source occurrences/global overlap, clipped segment,
disabled/suppressed/outside/hidden targets, no-net movement, Escape/hide/mode/frame
cancel, stale revision/session, invalid release and History limits. Committed
bytes/revision/History are unchanged during preview/refusal. Public Window arm
control, native/Host Save/cold read and legacy Guide/Align regressions remain
separate checks. No core/native schema or background semantics change.

## Implementation and evidence

Canvas exposes one explicit occurrence arm operation. Window's existing Guide
selector offers `Drag local position once…` or `Drag position override once…`.
The active frame, original authored Guide/source and Template target are captured.
The normal legacy global mode remains persistent and unchanged when not armed.
Scoped hover uses the same clipped hit test as press.

Release evaluates its final pointer coordinates before committing. Pan interruption
cancels the scoped gesture before changing drag kind; this repairs the independent
review finding that a pan release could otherwise commit an earlier valid preview.
Escape also clears armed-only state. Returning to the starting position resets the
preview from the committed document, preserving override absence and History.

Test-first build failed only for the not-yet-added arm API. Dedicated Canvas tests
cover inherited/local/source and both axes, negative origins, nonunit/frozen view,
clipping and missing/disabled/hidden/outside refusal, same-source and global overlap,
no-net/no-motion/orthogonal input, release-only movement, invalid last preview/release,
active/armed-only cancellation, external revision/source edit, History rejection and
repeated one-shot edits. Window/Host tests exercise actual selector/button routing,
stale captured UI refusal, native Save and fresh-process validation. No core/native
schema change. Exact final counts and aggregate receipt follow below.

Independent read-only review found and the implementation repaired the pan bypass
and scoped hover feedback. No further blocking source issue was reported. Windows,
visual hands-on and formal MCP runtime acceptance are not claimed by offscreen tests.

### Final cloud receipt — 2026-10-01

- Dedicated Canvas78 and public Window/Host29 checks PASS (107 checks).
- Focused8/8 Guide/Grid/Canvas contracts PASS; final all-target desktop Release
  build PASS. Exact production/test source digests rechecked after build.
- Full serial67 CTest:40 pass,1 explicit WIC skip,26 fail. Failing-name set is
  identical to the retained65-test inherited-Grid baseline; no added/removed failure.
- Real-process smoke PASS, including fresh-process native validation and SVG anchors.
- Full aggregate/Windows/hands-on/MCP acceptance remains unqualified. The26 retained
  failures comprise DirectWrite10, WIC4, Windows drive-path3, local-server7,
  unavailable external fixture1 and cross-platform SVG golden one-ULP1.
- Evidence: ignored `build/d0-evidence/scoped-guide-{all-build,all-tests,focused-final,smoke}.log`,
  `scoped-guide-ui.log`, `scoped-guide-candidate.sha256`. Native0.77 unchanged.
- Next eligible scope: Template-aware ordinary Margin/Grid Inspector family isolation,
  independently of the unanswered background product choice.
