# R07 — preserve exact Macro Amount edits

Base: `630ba88561b9eacb8fa5a84aaa9bb47a87eb25f5`. Native 0.77 unchanged.
This is a local, uncommitted Macro Amount control repair. No core, schema, font,
or unrelated Inspector semantics changed.

## Reproduced defects

A real production `Window` and `Session`, loaded from an encoded/decoded native
fixture, reproduced the precision defect before production edits. Focusing then
blurring Amount changed `12.34567` to `12.346`, revision `0 -> 1`, and changed the
authored document/history. This happened both for a pinned default with no local
override (an override was created) and for an existing override. The old control
used three decimal places, then compared its rounded value to the exact initial
model value. `red.log` preserves both failures; the initial test source is
`red-test.cpp` in the evidence directory.

After the precision repair, a separate dirty-Reset regression found that entering `98.7654321` and clicking
Reset first committed the text on focus loss. The Inspector rebuilt, then the
stale Reset could not remove the override. `dirty-reset.log` preserves this
failure. A rejected TabFocus-only experiment still moved focus to the scroll
area and retained the failure; it is not in the final source.

## Bounded repair

- Reuses the existing exact Inspector pattern: `QLineEdit`, `g17` display,
  modified tracking, finite `QString::toDouble` parsing and exact equality
- Unmodified focus/blur/Return and equivalent numeric input are no-ops; no
  tolerance or rounding threshold hides a changed value
- Actual edits still use one canonical `SetMacroOverride` for the frozen
  ObjectID, instance ID, PublicParamID, session and revision. Core retains the
  signed `[-1000000, 1000000]` document-unit range
- Widget-local Escape restores pending text only. No global shortcut changed
- A left-mouse focus transfer to this field's own Reset defers the edit to that
  gesture. Reset clears the draft and issues one `ResetMacroOverride`. Normal
  button focus policy, Tab editing and Space activation remain. Cancelling the
  mouse gesture preserves the pending text for a later ordinary edit. An
  editor-lifetime focus-change connection handles Qt consuming the deferred
  `editingFinished`; modified tracking prevents a duplicate commit
- The existing Effects Macro assertions use the exact line editor and a
  fractional value, including the following Detach-copy assertion

## Focused coverage

`macro_amount_ui_tests` uses the real Window, Session commands, native codec and
cold Host save/reopen. Both default and override fixtures cover no-op focus,
Return, retyped/equivalent values; intentional Enter and blur edits; exact
canonical document/history comparison; one Undo/Redo; stable source/instance
identity; native bytes; Reset; Escape; stale revision and replaced Session;
invalid/nonfinite/out-of-range/underflowed input and subsequent correction.

Values include ordinary signed fractions, `1e-20`, `-1e-100`, minimum normal,
both signs of the minimum subnormal double, zero and both range endpoints.
These tiny values are accepted by the existing core; actual UI parsing and
cold reopen are checked, rather than assuming decimal or percentage semantics.
Macro v1's refusal of linked/expression-driven node defaults remains atomic.
Dirty Reset additionally covers invalid input, stale refusal, cancelled
press/release, later blur, keyboard Tab and Space.

The original `12.34567` no-op cases preserve revision `0 -> 0`, history states
`1 -> 1` and these exact before/after SHA256 native hashes:

- Pinned default: `e2294c302e36b5134939c0497997ecb214f7339e21319d771e3b99ffe1b15d70`
- Existing override: `59bbe68ca669612b361baed0c7336f26de13b2dac1b4963311bf9a9628ff38a7`

## Initial Linux verification and limits (2026-10-02, 16:25 UTC)

- Four-target Release build passes (`macro_amount_ui_tests`,
  `effects_panel_tests`, `window_tests`, `macro_tests`)
- Final `nect_desktop` relink passes against the frozen source/library. Executable
  SHA256: `e492f2c1044290ac3b41e8711b3982808a389ee08f2150d6621eceaa07ac7288`
- Focused real-widget suite: 478 checks pass on offscreen QPA and another 478
  on minimal QPA; the registered offscreen CTest also passes
- Core Macro contract: 115 checks pass
- Bounded CTest set: 2 pass / 2 fail, with the two baseline limits below
- `git diff --check` passes; no full-suite or all-target pass is claimed

Final commands, outcomes and exact artifact hashes are recorded in
`build/d0-evidence/macro-amount-precision/manifest.json` beside the logs.
The source build uses the existing Qt 6.5.3/Boost dependencies and Release
`build/d0-core`, serial `-j1` for `window.cpp`.

The broader `window_interaction` and `effects_panel_interaction` suites must not
be called green: the former stops at the Linux DirectWrite boundary; the latter
stops at the earlier Asset Favorite absolute-drive-path case before its Macro
section. The same errors are present in the baseline
`build/d0-evidence/p02-d-caption-tight-root-full.log`, SHA256
`1f08bb906f32bb83b87c8251eb5d1681b6c8b5a8964f690f0ca908c1a00625ca`.

At this initial Linux checkpoint, offscreen/minimal QPA tests provided automated
widget/semantic evidence, with no manual GUI, Windows, screen-reader or whole-REQ
acceptance. Live MCP was not run. Narrow Windows follow-on evidence appears below.
No commit, push, merge, PR/Notion update, source publication, new dependency or
font-worktree operation was performed. The pre-existing Repeater test/receipt
patch was left unchanged; its hashes are in the manifest.

## Narrow Windows qualification follow-on (2026-10-02)

The parent read the completed task `01a0fd72-d963-77c0-b59d-eb4ca51570be`, whose
final result was recorded at `2026-10-02 16:45:13 UTC`. The following is that
task's reported Windows evidence; this documentation update did not execute or
change its source/build.

The Windows private copy consists of base
`630ba88561b9eacb8fa5a84aaa9bb47a87eb25f5`, the frozen Macro candidate and the
existing Repeater fixture patch. It is not a new Git HEAD. Source:
`C:/Users/shiro/Documents/Codex/2026-10-03/task-2/Nect-validation`; the build is
the sibling `Nect-build` directory. Native 0.77 remains unchanged.

All four normalized-LF source SHA256 hashes match the frozen candidate:

- `src/desktop/window.cpp`: `d3814eda0f8a66966be70166101a7b064064aa248be178a9cf31c37b9cd2c7db`
- `CMakeLists.txt`: `d34f2bd59de8e4f88f19da59afa153e50f623ac6333fc2c56ff06473228be985`
- `tests/macro_amount_ui_tests.cpp`: `94f5aefd70a4fcebd7510aa14227ead4db42bac581469a3f47e117a5d2e6d7c4`
- `tests/effects_panel_tests.cpp`: `4046934edd2829cf989e5328ac198805f59de5f63abf292ca917efe4bb63adbf`

Windows MSVC 2019 Release qualification:

- Focused-target build: PASS
- Direct focused test: 478 checks PASS, natural process exit 0
- Registered `macro_amount_ui_interaction`: CTest 1/1 PASS, 17.59 seconds
- No other suites were run in this follow-on

Reported Windows artifact SHA256 hashes:

- Focused test binary: `7D4D42C5FB092614EFD88E40ACCBB718CE2C7BEF2DF3CE4300886539F381D2E5`
- Configure log: `B7339E81BEFBE4CBD42755E4D2476CE63B5D7B49D17D73A6EDE228E6E700D062`
- Build log: `2E70E14647BD085D97A9C8B3F690FA8C475A45F0CD042D2CB644EDEF56E80478`
- Direct-test stdout: `05CC90604080955B16330CBE980A13A775C26EFE42110A4BF2EAF6AB0769FE49`
- CTest stdout: `3442E56F09F3BE490667158D9084711C932E89E04E58965651FDA06782E192B9`

This qualifies the narrow automated Macro Amount test on Windows. It does not
turn the earlier Linux failures into passes or establish a Windows full-suite,
manual/native-GUI, screen-reader, live-MCP or whole-REQ result. Own-cloud real-GUI
work uses a separate frozen binary; its results are not claimed in this addendum.

### Receipt history

The initial receipt is preserved byte-for-byte at
`build/d0-evidence/macro-amount-precision/receipt-linux-frozen.md`, SHA256
`dbb0e4eb9a96a0eca3387e786ddd0e46e1e10cd390de36d95553b1e190809e47`.
The original `manifest.json` remains unchanged, SHA256
`26f371521e8a60a1c14715dad47e5aa277dbb4f9f0c1834c1ad6ded60385e7a3`;
its receipt hash describes that initial snapshot, not this expanded document.

## Actual own-cloud GUI and final fan-in — 2026-10-02

The immutable desktop `e492f2c1044290ac3b41e8711b3982808a389ee08f2150d6621eceaa07ac7288` passed actual styled Linux X11 interaction: precise default/override focus and blur, intentional fractional Return/blur edits, one-step Undo, default Escape and dirty Reset. Reset produced only the Reset history entry, no intermediate typed-value commit; one Undo restored the original precise override. Nine whole-native-byte comparisons include three independently generated canonical-command outputs. Root verified all41 artifact hashes, all nine comparisons, native JSON and immutable executable, and inspected Reset/Undo screenshots. App closed; no source repair.

GUI report SHA256 `c3944a2b99e508676eca99af2a6d21a9e324df0f654a3d6504025a4a82045f1b`; manifest `65073b7f604c5f6c7009992f9c9501a22f1a710d238d50a9b9d20364c6f68957`. Evidence contains28 raw screenshots including excluded setup/driver diagnostics: type-text-only input did not mark the Qt control modified, so successful edits used genuine key events; an incidental wheel change in an initial override session was discarded and the pristine fixture reopened. No held-mouse, Windows GUI, screen-reader, live MCP, arbitrary-DPI or whole-requirement claim.

After explicitly rebuilding/relinking all ten selected binaries, root's21-test Macro/angle/Repeater/utility/compact fan-in passed. Final test log SHA256 `edced6080d8fcd49392ef8ad70d1ae01cae74f9fe8dfed2b68729b0d222d1ec6`; relink log `3ae80c5ebb1e5599951c97f2d4a9301df96efa02143cd9966cafe9ea736de07f`. This supersedes the earlier fan-in's previously linked angle binaries for exact-candidate qualification. Full-suite baseline limits above remain.
