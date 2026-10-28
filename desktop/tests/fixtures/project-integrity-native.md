# Disposable project integrity fixture

Use `project-integrity-native.json` and `project-integrity-import.svg` with the candidate EXE whose SHA256 is supplied by the integration owner. This fixture does not certify any previous ZIP.

The JSON contains two outer Compositions, a synthetic 80×80 RGBA image, the existing repository's `NectJPFixture-Regular.otf` test font embedded as `font-integrity-fixture`, schema 50 Feather 4, and one `Existing shared value` node preset. It contains no user artwork or Recovery state.

1. Open the JSON. In **Active - import target**, select the **Feather artwork** layer. Fit all graph nodes. Select Source image, Remove green, Mask rectangle and Shape mask by clicking the first title and Shift-clicking the other three.
2. Enter **Feather UI preset** in Preset name and click **Save preset**. Confirm it appears in Presets. Undo and Redo should remove/restore that single preset. Apply it and confirm the copied Mask still has Feather 4. Undo/Redo should remove/restore the four copied nodes.
3. Choose Import → SVG document. Cancel the unsaved-changes prompt; repeat with Escape. Both must preserve the current project. Then repeat, choose Discard for the replacement guard, and choose `project-integrity-import.svg`.
4. Confirm only the active Composition changes to a dark blue rectangle. Switch to **Sibling - preserve assets**. The red/green feathered image and Japanese fixture text must still render. The existing and newly saved node presets must remain available.
5. Undo/Redo the SVG import and confirm both states. Save As to a new filename in a dedicated acceptance output folder. Reopen that saved copy, switch between both Compositions, and apply the Feather preset again.

Record the exact EXE SHA256, candidate source SHA, outcome and screenshots. A failure is not a PASS. Never overwrite this input, a user project or an existing Recovery candidate. The accompanying Chrome test verifies browser downloads and input-file reopen; native OS dialogs and cold restart require separate execution.
