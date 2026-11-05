# Key color · Experimental — retained-recipe editor

Open **Shared colors → Key color · Experimental**. This is an opt-in editor for
an **already retained** `keyColorRecipe.committedSnapshot`, not a new default
palette. Without a receipt, the same launcher opens a research-only detached
preview with initially blank HEX, alpha and candidate selection. The compared
`research-triadic-offset` and `research-neutral-start` v1 coefficients are
explicitly inspectable, not adopted defaults. Preview creates no shared slots,
receipt or History entry. After an explicit B Preview, the user may add five
trial shared colors and a receipt in one Undo; A remains Preview-only. B uses
`experimental-affine-neutral-text-v1`, fresh draft-stable IDs and the same
closed initialization preflight. Inputs, candidate
changes and composing invalidate the preview; owner, revision or Composition
changes make the draft stale until it is closed. The neutral-start candidate
keeps C1 exactly and starts all derived colors with zero chroma; no derived
chroma controls are provided. Coverage and contrast are not evaluated. This
does not automatically bootstrap a palette or convert the legacy wheel. On
reopening a B trial receipt, derived Δr remains stored but its editing controls
are hidden; other profiles and legacy receipts keep their existing controls.

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
  active targets only. It does not modify Canvas, authored data or History.
- Any input/change invalidates that preview. Blank, malformed, nonfinite or
  out-of-range alpha anywhere disables Preview/Apply and removes the old
  comparison. Apply also rechecks every input and the exact preview fingerprint,
  so a silent/unobserved input change cannot dispatch the old valid result.
- **Apply · 1 Undo** calls `prepareKeyColorRecipeApplication` again, then sends
  exactly `apply-key-color-recipe-snapshot` with the closed `{ snapshot }`
  payload through the real app dispatcher and common Session. It does not inject
  `layerId` or compose public set/retain commands. A null/undefined/false dispatch
  result is an error, not a success notice.

A changed Apply atomically commits active literals and the historical receipt in
one Undo. Equality is a no-op preserving revision, History and pending Redo. Every
active legacy-locked target blocks the whole operation, even an equal Apply;
the editor never unlocks, skips or partially updates targets. Dormant locks do
not block active targets. Old `colorDesign`, Absolute Chroma/Neutral settings,
roles, IDs, bindings and unrelated/dormant shared values remain untouched.

The receipt is still **historical, not a live driver** after Apply. A later manual
shared-color or legacy wheel edit may differ from it without rewriting it.
Opening the editor reads retained intent, not a reverse-engineered live color.
Native JSON saves/reopens both the changed literals and receipt; it stores no
pending draft, preview or error. No solver or schema change is introduced here.

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
refusal and narrow layout. It blocks font requests and uses only its generated
synthetic fixture, not user documents.

The C1 HSL addition passed this browser acceptance in Windows Chrome, including
plane/keyboard/numeric edits, gray hue, cancellation, one-Undo Apply, browser
save/reopen and narrow layout. The earlier Linux sandbox startup refusal was not
retested by this change. Windows Tauri dialogs, WebView2/native exit/restart,
human comfort, final presets/dark coefficients and ColorF32/Field rendering remain
separate acceptance limits.

See [retention and Apply contract](key-color-recipe-retention.md) and
[pure model contract](../../docs/key-color-recipe-contract.md).
