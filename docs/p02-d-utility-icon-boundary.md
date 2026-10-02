# P02-D Utility Strip binary icon closure / Packet A

Status: CLOUD AUTOMATED QUALIFIED / WINDOWS OBJECTIVE GUI O1 PENDING. Base `bba27ee8c5b2e7def64310f9b8efdd7aeb48e04b`.
Dot owns the planned cloud implementation. The active isolated Windows validation/
repair lane explicitly confirmed no Window source repairs and released the narrow
constructor/icon-helper plus dedicated test scope to Dot on2026-10-02. Existing
state owners and other Window functions remain outside this presentation scope.

## Accepted authority and observed gap

Canonical [UI Closure Packet A](https://app.notion.com/p/3eafd279a6f38185bce6e73fcb46b0df)
and Confirmed/Should [REQ-164](https://app.notion.com/p/3e0fd279a6f381fcab0cd396763100fc)
/ [REQ-179](https://app.notion.com/p/3e0fd279a6f38190821bf2926a9ee650) require binary
quick controls to be icon toggles with persistent ON/OFF distinction. Current
`utility-show-guides`, `utility-show-grid`, and `utility-snap` are TextOnly with null
icons. An unchanged-source offscreen baseline capture confirmed all three null icons
and native revision0 while Guides OFF / Grid ON / Snap ON. This is not real-GUI evidence.

## Narrow scope

- Add repository-native geometric icons using existing Qt Gui facilities, without
  external assets/fonts, another rendering owner, or a new animation framework.
- Change only these three binary presentations in the Utility Strip constructor.
  Fit, Setup, Keys, zoom and output readback keep their existing roles/presentation.
- Preserve Canvas/action ownership and existing state synchronization. In particular,
  Snap's icon must belong to its existing default QAction so ActionChanged updates
  cannot erase the button icon. No new Document field, command or native version.
- Keep existing restrained palette, persistent checked styling, hover/focus/disabled
  distinction, meaningful tooltips and accessible names. Fixed logical hit dimensions
  must not change with text/state/hover/focus. Supply crisp high-DPI icon rendering.
- Guide and Grid visibility remain independent of each other and Snap categories.
  Hidden overlays remain eligible Snap targets under the existing rules.

## Test-first acceptance

1. Geometry-only Window fixture asserts nonempty icons and icon-only style for the
   exact three controls; semantic action/state remains unchanged and Snap action ↔
   button parity survives repeated changes and refresh.
2. OFF / ON / hover / focus / disabled render differently while geometry remains
   fixed. ON/OFF distinction is visible without hover. Names remain available to
   tooltip/focus accessibility; disabled activation does not toggle.
3. Repeated pointer/keyboard/action/Canvas synchronization changes preserve exact
   Document/native bytes/revision/History. Existing zero-revision and narrow Utility
   Strip behavior is retained.
4. At1000×650 every control is reachable. Scrolling is required when content actually
   overflows, not as an artificial assertion that a compact strip must overflow.
5. Inspect generated normal/high-DPI state captures. Automated offscreen render proof
   is separate from packet-required objective normal/high-DPI real-GUI O1 receipt,
   which the Windows lane must establish on the same integrated source SHA.

No global shortcut/workspace redesign, authored state mutation, angle-dial Packet B
or general hover census Packet C is included. Do not mark whole REQ-164/179 verified
until G2 checks their complete acceptance and the required objective GUI evidence.

## Implementation and verification receipt — 2026-10-02

The three controls now use distinct Guide, Grid and magnet/Snap geometric icons,
20 logical pixels at DPR1/2/3, all QIcon modes/states. Each fixed36×32 hit target
retains existing checked/hover/focus/disabled styling and accessibility. Snap's
existing QAction owns its icon. No state-owner, command, native or schema changes.

- Red baseline failed at `binary utility has an icon` as expected.
- `utility_icon_ui_interaction`:277 checks PASS at DPR1.
- `utility_icon_ui_hidpi`:278 checks PASS at actual DPR2.
- Every mode/state/DPR has exact physical size and nonempty transparent-backed
  pixels; identities differ pairwise. Captured glyphs must overlap their solid
  alpha mask with visible contrast. Snap captures use the authoritative QAction
  and verify Canvas parity. View interactions preserve exact native/revision/History.
- Normal/high-DPI contact sheets were visually inspected: all15 cells per sheet
  contain visible glyphs; ON/OFF, hover, focus and disabled remain distinct.
- Review exposed a false-positive oracle: regional `QWidget::grab(rect)` produced
  blank hover cells with the existing graphics-opacity effect. Capturing the whole
  Window then cropping restores the actual composited glyph. The new mask/contrast
  oracle rejects blank captures. No production HoverFeedback change was made.
- Independent read-only review found no remaining blocking source/test issue.
- All-target build PASS. Full74 tests:47pass/1skip/26fail. The26 failing test names
  exactly match the bba27ee baseline; this is not a whole-suite pass. Existing
  DirectWrite/WIC/path/process and SVG golden constraints remain explicit.
- Fresh-process native/SVG smoke PASS; smoke does not test GUI or formal MCP.

Ignored local evidence SHA-256:
- `utility-icon-all-build.log`:2b65a1c45a50863fa24d0bb60fbb240ef097781fade5e36bb62160b1f5b04797
- `utility-icon-all-tests.log`:9cc37f9fbf0bad9223f2d3275e7af5e46ffc4f347d64fe92ef669578c9ac261a
- `utility-icon-smoke.log`:7cd237678e4001117718f685f4c612cbbbcb217b79273c1ad43af6dfc8986cb4
- `utility-icons-1x.png`:b1b8086eae2e22c9ad488da784246ce071d214733ec086e8c27e8501f91adcd7
- `utility-icons-2x.png`:eb6a9b5e3a45ce13407bf3c79fdfeae260ca77327ea63cf6b79b07fec7f06ea1

Windows task `01a0f9d8-37f9-729f-ae70-b5abf075608e` remains on its immutable
bba27ee baseline. Hand it the synchronized candidate SHA for real-GUI normal/high-DPI
O1 after its current build boundary. Do not conflate these offscreen tests with that
receipt or promote whole REQ-164/179. Packet B remains independently eligible.
