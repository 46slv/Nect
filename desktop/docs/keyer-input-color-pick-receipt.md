# Keyer input picking verification

Base: PR68 `7797e6695ce06e69973c15d1444514d495eb6d0d`, branch
`codex/selected-anchor-spacing`. Product/build source:
`7f9817b1bde34c49958905cbe5acd04b4c28f0e9`. The subsequent receipt/test-only
commit does not change bundle/native inputs. [Brief](keyer-input-color-pick.md).

Product delta:4 files,298 additions, no removals (`main.js`, scoped `style.css`,
`keyer-input-pick.js`, `keyer-input-picker.js`). The existing Keyer evaluator,
image resolver, schema, Mask, dither and export implementation are unchanged.
Selected Key color gains Pick from input; driven fields say Pick base from input
and retain expressions/connections and stored alpha through existing Color edits.

## Verified

- 38 focused checks, zero failures/skips, in six files: `keyer-input-pick`,
  `keyer-input-pick-dom`, `image-effects`, `color-value-expressions`,
  `color-value-dom`, `image-dither-dom`. New picker coverage is8 model checks
  and6 DOM checks. Final log: `evidence/focused-complete.log`.
- Existing resolver evaluates the selected Keyer's upstream Image only. Earlier
  Keyer effects apply; selected Keyer does not. Straight RGB from a partial-alpha
  texel is preserved; alpha0/outside are not adopted. Floor and half-open image
  bounds, no clamp, stable base editing, locks/missing/unsupported input, valid
  same-size image replacement, stale target/revision/Session and late events,
  cancel/Escape/blur/pointercancel, neutral hover/start, pointer exclusivity,
  one Undo/Redo/no-op and asset/JSON/SVG preservation pass.
- The intrinsic-pixel preview uses its SVG screen CTM inverse. Image position
  and nonuniform size do not alter the sampled texel. Model tests cover rotated/
  nonuniform screen matrices and singular/unavailable/3D refusal. Canvas view
  transforms and artwork placement are outside this mapping; no new Image-to-
  PaintedPath/Composition-to-Keyer conversion is introduced.
- One successful native Tauri/WebView2 CDP synthetic flow on SHIRO-WS uses a4x2
  synthetic PNG, two Keyers, placed at[20,30], size[284,118], canvas320x180.
  The upstream removed green pixel is rejected. Pixel[1,0] picks `#0c2238`
  despite its partial alpha/checkerboard; output alpha changes128 to0, opaque
  red stays unchanged. Start/hover are document-neutral; one click/Undo/Redo,
  fresh-state repeat no-op, exact JSON/SVG reload and native Canvas PNG RGBA
  reload equality pass. No page errors. Both screenshots were visually inspected.
  Success files: `evidence/native-verified/` and `native-verified.log`.
  PNG/SVG full RGBA SHA256:
  `0dedb529bf246c24074f44752950dd92c4052e9789b167c98337a0832d2976c7`.

The PNG success route is generated SVG -> native WebView Canvas PNG, written
through the CLI. It is not an actual successful Export PNG/native save. The
original test attempted to intercept save_png, but Tauri's invoke property is
nonwritable/nonconfigurable. It was not overridden and no protection was changed.
The final test contains no IPC replacement or native export click.

Initial test preparation corrected the required image asset ID prefix, DOM
pointer selection, expected export-error representation for missing inputs,
normal Export-menu access, and fresh-state no-op comparison after Undo/Redo
recreates the Session. Assertion coverage was retained. Native pixel centers use
odd placement scales to avoid ambiguous half-pixel interpolation in the fixture.

## Build, saved output and remaining native cleanup

Frontend build passed in4.45s with the existing chunk warning. Locked offline
release/custom-protocol Cargo build passed in41.24s using the existing D target.
Heavy outputs, runtime/profile and child TEMP/TMP use
`D:\Nect-codex-output-20261022-task13-f336c94f2b714cd5bc73d6ca646c8f2e`.
C retains source/dependencies/Cargo home and the small fixed frontendDist.
No deletion or capacity hold. All26 prior EXEs and301 sealed files match before
and after the build/checks.

EXE:10,010,112 bytes, SHA256
`3f96eb6edff27f22b51acfc7895513f910950eb1a35554ea260d37f10f45e317`.
Candidate: `D:\Nect-codex-output-20261022-task13-f336c94f2b714cd5bc73d6ca646c8f2e\keyer-input-artifact-7f9817b`.
The external manifest binds actual package files/final remote identity.

Successful test PID125420 exited normally after preserving its fixture. Initial
menu-preparation PID122776 also exited. PNG-save-attempt PID139120 remains alive
in `runtime-7f9817b` (CDP9481), after the native save route was entered. Its normal
close attempt is unconfirmed/pending; no retry/forced termination or alternative
OS-control route was used. The failure fixture is saved at
`keyer-input-pick-mvizo4de/evidence/native-final/failure-fixture.json` under the D
root. OS dialog appearance/state cannot be inspected by this executor; normal
OS dialog input is the missing dependency for resolving that test instance.
Its fresh acceptance profile is isolated from user AppData/Recovery; no existing
user instance/document was closed or overwritten. Exact status is recorded in
`evidence/close-capacity.json`.

Physical/OS input, actual OS Save/Open/PNG export and normal OS cold reopen remain
unverified. No new feather/luminance mask/evaluator, external images/services,
main merge, release, installer or Library upload.
