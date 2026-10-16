# Desktop workspace verification

2026-10-16. Isolated branch `codex/desktop-workspace-20261016`; base `52365d3653bda02d3bf238a606d80f7a398ff923`. Code and this evidence share the checkpoint commit. No full legacy suite was replayed.

## Two-issue layout repair after 08de238

Sole source writer took over in the independent `task-12/nect-desktop` clone after confirming the remote branch and draft PR23 both at `08de238a85a545ec81c9668007334dd1cf050e0f`. The task-11 source was only read; its review application PID84564 remained responding and was not operated.

- Windows x64 Release build of `nect_studio` and the focused test target: PASS, installed Qt6.5.3 / VS2019. Builds held the existing zero-timeout `Local\NectBuild` mutex; all leases were released.
- [26 layout-only Qt-event checks](layout.json): PASS. Nodes open/close repeatedly refits an explicitly fitted canvas, while real wheel zoom and middle-button pan retain their mapping. Keyboard Fit restores tracking. The artboard title clears the breadcrumb after refitting.
- The hint now lives in the fixed 22 DIP status bar outside the canvas. The header owns zoom; no hint or duplicate zoom readout paints over artwork. Narrow hints elide and retain the full tooltip.
- At 683 and 799 logical width Inspector measures 240 DIP; at 800 and 1440 it measures 280 DIP. Real sidebar button events return space to canvas and preserve mutual exclusion below 800, including reopening an Inspector collapsed across the breakpoint. Numeric fields retain two aligned columns, 28 DIP height and 13 DIP type.
- Those view/layout events leave the exact authored Document, Session revision, History count and dirty/save state unchanged. No model, command, native codec, Undo or persistence implementation was edited. The previous 23 semantic workflow PASS results are reused from 08de238, not replayed.
- Three final widget renders were regenerated and visually inspected: [pipeline](pipeline.png) (1440x900 logical, 100%), [683 logical](1024x900-150.png) (683x600, DPR1.5, actual1025x900 pixels) and [wide](1440x900-100.png) (1440x900, 100%). Each has a matching `.png.json` dimension sidecar. The other blank/width/scale images remain first-checkpoint evidence.

Focused test reproduction from the repo root after `scripts/build-studio.ps1`:

```powershell
$env:QT_QPA_PLATFORM='offscreen'
$env:QT_QPA_FONTDIR="$env:SystemRoot/Fonts"
$env:QT_SCALE_FACTOR='1'
./build-studio/Release/studio_workflow_tests.exe --layout-only ./docs/desktop-workspace/layout.json ./docs/desktop-workspace/rectangle.nect
```

Final local binaries in `task-12/nect-desktop/build-studio/Release/`:

| Artifact | SHA-256 |
| --- | --- |
| nect_studio.exe | D07CA622EB7BE9AF11115469F7F7E7A7244C86761544E8FAAE690CA05A6935B8 |
| studio_workflow_tests.exe | 1EA1C62B66BAC8997FAB758D0AE8B1FB30DB8C0E1F4E6B3FF399866E745EBD37 |

No native screenshot/recording was attempted again. The existing MXC HRESULT0x80070003 capability failure and physical display/IME/subjective acceptance boundary remain. There is no pending approval or denied action. Next: user reviews these two local repairs using the new binary when ready; preserve the still-open first-checkpoint app until then. No feature expansion, merge or release is part of this repair.

## First-checkpoint evidence (08de238)

- Windows x64 Release build of `nect_studio` and `studio_workflow_tests`: PASS, Qt6.5.3 / MSVC14.29 (VS2019).
- Focused workflow: 23 checks PASS ([receipt](workflow.json)). It drives real widgets through Qt input events: blank, rectangle drag, one History entry, canvas movement, W/H/fill, Undo/Redo exact state, Escape draft cancellation, invalid-size atomic refusal, native save, independent Host reopen and continued editing, rectangle gesture cancellation, empty Inspector, and menu row geometry.
- The rectangle fixture saved by the workflow is [rectangle.nect](rectangle.nect). The independent Host reopened the exact authored Document and saved a subsequent width edit. This does not claim process crash/power-loss durability.
- All four requested width/scale source renders were visually inspected. Sidecars record actual logical and pixel sizes. At150%, 683 logical pixels round to1025 physical pixels for the 1024-width request. No manual double scaling or OS setting changes.
- Existing `examples/gradient-ornament.nect` opens/renders through the production codec/Canvas; its source file was not modified. Advanced gradient editing controls remain unmigrated.
- Native Windows `nect_studio.exe` launched with the rectangle fixture and owned recovery path. PID84564, input-idle true, Responding true, MainWindowHandle59969052. This establishes launch/responding window evidence only.
- Canonical Nect and Nect-SVG worktrees remain clean; Nect-SVG remains `feat/operable-node-editor@107a154e8d5a887f4bc551ab7bcafbc88773da87`. No Nect-Cloud changes.

## First-checkpoint binary binding (08de238)

Local output: `build-studio/Release/` (Qt runtime deployed beside the executable).

| Artifact | SHA-256 |
| --- | --- |
| nect_studio.exe | C7112E0427E5C0E0FCB4451A52475917419B0E143442696BF16FB2E895DCFDC4 |
| studio_workflow_tests.exe | 2BB6EC9EFD9195017434C62C2B6E06B83685CB4EA99CDD5AB9194B981972EB0E |

## Remaining boundaries

Native screenshot/short recording failed at runtime initialization: MXC launcher cannot locate required Windows platform directory, HRESULT0x80070003. No approval is pending and no action was denied; the capability failed. It was not retried without new host evidence. The committed images render the real Qt widget tree through the offscreen platform, with system fonts loaded read-only from Windows Fonts. They do not show OS titlebars, prove physical display DPI/IME, or certify subjective operation comfort.

Nodes is a read-only projection of actual retained source/processing/transform/output, sharing selection and Inspector focus; arbitrary graph wiring is not implemented. Numeric text editing/Enter/Escape are supported; horizontal numeric scrubbing is not yet implemented. Advanced native controls and Nect-SVG graph JSON migration remain explicit follow-on work.
