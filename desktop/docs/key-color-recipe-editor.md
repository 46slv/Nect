# Key color · Experimental — trial and retained-recipe editor

Open **Shared colors → Key color · Experimental**. This is an opt-in editor for
an existing retained or linked trial `keyColorRecipe.committedSnapshot`, not a
new default palette. Without a recipe, the same launcher opens a research-only detached
preview with initially blank HEX, alpha and candidate selection. The compared
`research-triadic-offset` and `research-neutral-start` v1 coefficients are
explicitly inspectable, not adopted defaults. Preview creates no shared slots,
receipt or History entry. After an explicit B Preview, the user may add five
trial shared colors and a receipt in one Undo, or explicitly choose a linked
trial; A remains Preview-only. The linked choice is not a default and does not
convert any existing palette. Choosing it displays the older-reader
incompatibility before creation; the trial can later be explicitly unlinked. B uses
`experimental-affine-neutral-text-v1`, fresh draft-stable IDs and the same
closed initialization preflight. Inputs, candidate
changes and composing invalidate the preview; owner, revision or Composition
changes make the draft stale until it is closed. The neutral-start candidate
keeps C1 exactly and starts all derived colors with zero chroma; no derived
chroma controls are provided. Coverage and contrast are not evaluated. This
does not automatically bootstrap a palette or convert the legacy wheel. On
reopening a B trial receipt, Assort and Accent expose draft Δr. Exact gray C1
requires active chroma intent to be zero before Preview or Apply; pending
values are preserved. This also checks hidden active custom-role chroma intent
and nonzero chroma offsets. Near-gray hue is not evaluated. Neutral/Text and other B
roles keep Δr hidden, dormant rows remain unchanged, and legacy controls keep
their existing behavior.
Each active B derived color can reset its draft ΔL to zero; this invalidates
Preview and changes the document only after a new Preview and Apply.

B trial status lists saved and proposed OKLCH L and ΔL separately. Visible paint
share comes only from the existing current coverage snapshot matched by stable
slot ID; the refresh button rereads it. Missing, stale or unavailable results are
not converted to zero. Before creation, usage is not measured. Readability and
procedure checks are not evaluated here, and literal equality includes alpha.
L is abbreviated as an approximation for scanning; exact L, ΔL, stable ID and
diagnostics remain in each keyboard-accessible Details disclosure. Duplicate
visible roles or shared-color names receive a distinguishing ID-derived suffix;
all commands and comparisons continue to use the full stable ID.
For B recipes, pairwise derived model L order compares effective L
against the clamped zero-offset baseline by stable ID. Equality is exact; this
informational result does not judge appearance, contrast or whether Apply is allowed.

For a B trial, explicitly select two different stable IDs to check a
foreground/background pair. The advisory compares current shared literals and,
only while valid, the detached Preview separately. It composites foreground
alpha in encoded sRGB over an opaque background and compares the unrounded
ratio with the normal-text 4.5:1 reference. Missing, ambiguous, invalid or
translucent-background inputs are unknown. This is not an artwork accessibility
audit and does not change the document or colors.

The research C1 uses the same draft-only HSL picker as the retained editor.
Until a valid six-digit HEX is entered, HSL controls and the plane cannot edit a
color; no seed color is used for Preview. HEX spelling and alpha zero remain
literal until an explicit HSL change, and a hue-only edit on gray preserves HEX.

## Edit, Preview, Apply

- For explicitly initialized `experimental-affine-neutral-text-v1` receipts,
  Neutral/Text expose ΔL only. Their stored Δr remains visible as unapplied
  intent with a diagnostic when nonzero. Legacy solver controls are unchanged.

- C1 uses a **draft-only HSL picker**: saturation/lightness plane, hue slider and
  H/S/L fields, with a text HEX field and separate alpha. Opening, displaying HSL
  or cancelling never regenerates the exact six-digit HEX literal/case or alpha
  (including zero). Only an HSL edit that changes RGB replaces the draft HEX;
  valid direct HEX input keeps its spelling. Gray hue changes stay local until
  saturation/lightness produces another color, and do not set the recipe's
  separate OKLCH hue hint. Preview/Apply remain the only route to authored state.
- **Copy saved color** selects an existing shared color by stable slot ID and
  copies its exact HEX spelling and alpha into C1; it creates no reference and
  leaves the source untouched. The current Apply's active destination slots,
  including C1, are excluded. Names are disambiguated when needed. Selection
  invalidates an old preview and is refused during composition, a scrub, or a
  stale captured Session/Composition. The research draft can use the same copy
  without choosing or creating a candidate.
- Each active derived slot exposes finite stored **ΔL** and **Δr** intent. These
  are offsets, not clamped slider positions; the pure model owns effective
  clamps and gamut behavior. Dormant slot intent/results stay read-only and
  unchanged. Stable IDs, not names or array positions, identify targets.
- The retained preset, version, coefficients, solver/semantics versions and
  settings are read-only. Active derived alpha is shown as stored, read-only;
  Apply requires alpha1. Incompatible historical alpha is rejected, never
  silently made opaque. C1 alpha0 is independently valid.
- **Preview** explicitly computes a detached candidate using the existing
  `restoreKeyColorRecipe` / `editKeyColorRecipe` APIs. The comparison shows
  **current shared literals → proposed shared literals**, including alpha, for
  active targets only. Preview alone does not change Canvas, authored data or History.
- For a B trial, **Show proposal on artwork** temporarily displays the
  proposal through existing shared-color bindings. **Show saved artwork** restores
  the authored view while retaining the valid detached preview. Neither action
  changes the document or History; an expired preview restores the authored view.
- Any input/change invalidates that preview. Blank, malformed, nonfinite or
  out-of-range alpha anywhere disables Preview/Apply and removes the old
  comparison. Apply also rechecks every input and the exact preview fingerprint,
  so a silent/unobserved input change cannot dispatch the old valid result. A
  subsequent display-document read also expires the detached comparison and
  artwork projection if the fingerprint has changed.
- **Apply · 1 Undo** repeats preflight through the real app dispatcher and common
  Session. For a retained receipt it sends `apply-key-color-recipe-snapshot` with
  `{ snapshot }`; for a linked trial it sends `edit-live-key-color-recipe` with
  the captured palette revision and edited intent. The latter solves in the
  receiving Session. Neither path injects `layerId` or composes public set/retain
  commands. A null/undefined/false dispatch result is an error, not a success notice.

A changed Apply atomically commits active literals and the recipe packet in
one Undo. Equality is a no-op preserving revision, History and pending Redo. Every
active legacy-locked target blocks the whole operation, even an equal Apply;
the editor never unlocks, skips or partially updates targets. Dormant locks do
not block active targets. Old `colorDesign`, Absolute Chroma/Neutral settings,
roles, IDs, bindings and unrelated/dormant shared values remain untouched.

For a retained-only recipe, the receipt remains historical after Apply: later
manual shared-color or legacy wheel edits may differ from it without rewriting it.
For a linked trial, Key color edits update its saved recipe and owned shared-slot
values atomically, and existing bindings follow those values. Explicit Unlink
keeps the saved values and IDs as independent shared colors, with Undo/Redo.
Opening either editor reads saved intent, not a reverse-engineered color. Native
JSON saves/reopens the changed literals and packet; it stores no pending draft,
preview or error. Editing an existing packet does not change its schema.

## Cancellation, concurrency and legacy interaction

The native modal is appended outside the app's rerendered DOM. Text editing and
Tab retain native behavior, while editor/graph shortcuts cannot author through
it. Cancel, Close and Escape discard its detached draft without a command or
History change. IME composition blocks Preview/Apply; composition Escape/native
cancel does not discard the dialog. Saving/changing documents requires finishing
or cancelling the open edit first.

The Session owner and envelope are captured once at open, **never rebased** at
Preview/Apply. Concurrent edits, Undo/Redo or a different Session/Composition
invalidate the draft and disable Apply; cancel and reopen from current state.
Normal UI Composition changes are also protected by the existing active-edit
barrier. The launcher's pointerdown cancels a pending legacy live color gesture
before native blur/change can auto-finish it; opening the experimental editor is
not a request to commit that old gesture.

## Verification and limits

Run from `desktop/`:

```sh
node --test tests/key-color-recipe-panel.test.js tests/key-color-recipe-app-dispatch-dom.test.js
BASE_URL=http://127.0.0.1:5173 node tests/key-color-recipe-panel.e2e.js
BASE_URL=http://127.0.0.1:5173 node tests/key-color-artwork-status.e2e.js
```

The DOM tests use both real Session kinds, native serializers, real `main.js`
dispatch and a negative control restoring the old `layerId` injection. They cover
one-Undo Apply, no-op Redo, cancellation/IME, stale edit/Undo/switch, invalid old
previews, active locks/equality, literal/case/alpha0 and dormant/legacy identity
preservation. The color-only main harness removes unrelated startup font loading;
it does not replace the panel, dispatcher or Session.

The registered browser acceptance covers a real modal, keyboard/pointer launch,
before/after literals, invalid/cancel paths, one-Undo, Save download → app reload →
native JSON file-input Open, legacy gesture isolation, stale edits, lock/alpha
refusal, saved-color copy and narrow layout. Copy coverage includes stable IDs,
duplicate names, active destination exclusion, exact HEX/alpha0, preview
invalidation, simulated composition, stale/silent input, pending Redo, one-Undo
Apply and source preservation after reopening. It blocks font requests and uses
only its generated synthetic fixture, not user documents.

The C1 HSL addition passed this browser acceptance in Windows Chrome, including
plane/keyboard/numeric edits, gray hue, cancellation, one-Undo Apply, browser
save/reopen and narrow layout. The earlier Linux sandbox startup refusal was not
retested by this change. Windows Tauri dialogs, WebView2/native exit/restart,
human comfort, final presets/dark coefficients and ColorF32/Field rendering remain
separate acceptance limits.

The focused artwork-status acceptance uses an independent sandboxed Chrome and
a synthetic B trial to check explicit projection, restoring the authored view,
and expiration after an unobserved input change and browser resize.

See [retention and Apply contract](key-color-recipe-retention.md) and
[pure model contract](../../docs/key-color-recipe-contract.md).
