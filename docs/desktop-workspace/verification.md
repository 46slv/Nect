# First desktop workspace verification

2026-10-16. Isolated branch `codex/desktop-workspace-20261016`; base `52365d3653bda02d3bf238a606d80f7a398ff923`. Code and this evidence share the checkpoint commit. No full legacy suite was replayed.

## Established

- Windows x64 Release build of `nect_studio` and `studio_workflow_tests`: PASS, Qt6.5.3 / MSVC14.29 (VS2019).
- Focused workflow: 23 checks PASS ([receipt](workflow.json)). It drives real widgets through Qt input events: blank, rectangle drag, one History entry, canvas movement, W/H/fill, Undo/Redo exact state, Escape draft cancellation, invalid-size atomic refusal, native save, independent Host reopen and continued editing, rectangle gesture cancellation, empty Inspector, and menu row geometry.
- The rectangle fixture saved by the workflow is [rectangle.nect](rectangle.nect). The independent Host reopened the exact authored Document and saved a subsequent width edit. This does not claim process crash/power-loss durability.
- All four requested width/scale source renders were visually inspected. Sidecars record actual logical and pixel sizes. At150%, 683 logical pixels round to1025 physical pixels for the 1024-width request. No manual double scaling or OS setting changes.
- Existing `examples/gradient-ornament.nect` opens/renders through the production codec/Canvas; its source file was not modified. Advanced gradient editing controls remain unmigrated.
- Native Windows `nect_studio.exe` launched with the rectangle fixture and owned recovery path. PID84564, input-idle true, Responding true, MainWindowHandle59969052. This establishes launch/responding window evidence only.
- Canonical Nect and Nect-SVG worktrees remain clean; Nect-SVG remains `feat/operable-node-editor@107a154e8d5a887f4bc551ab7bcafbc88773da87`. No Nect-Cloud changes.

## Binary binding

Local output: `build-studio/Release/` (Qt runtime deployed beside the executable).

| Artifact | SHA-256 |
| --- | --- |
| nect_studio.exe | C7112E0427E5C0E0FCB4451A52475917419B0E143442696BF16FB2E895DCFDC4 |
| studio_workflow_tests.exe | 2BB6EC9EFD9195017434C62C2B6E06B83685CB4EA99CDD5AB9194B981972EB0E |

## Remaining boundaries

Native screenshot/short recording failed at runtime initialization: MXC launcher cannot locate required Windows platform directory, HRESULT0x80070003. No approval is pending and no action was denied; the capability failed. It was not retried without new host evidence. The committed images render the real Qt widget tree through the offscreen platform, with system fonts loaded read-only from Windows Fonts. They do not show OS titlebars, prove physical display DPI/IME, or certify subjective operation comfort.

Nodes is a read-only projection of actual retained source/processing/transform/output, sharing selection and Inspector focus; arbitrary graph wiring is not implemented. Numeric text editing/Enter/Escape are supported; horizontal numeric scrubbing is not yet implemented. Advanced native controls and Nect-SVG graph JSON migration remain explicit follow-on work.
