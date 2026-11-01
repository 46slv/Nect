# Key color · Experimental — retained-recipe editor

Open **Shared colors → Key color · Experimental**. This is an opt-in editor for
an **already retained** `keyColorRecipe.committedSnapshot`, not a new default
palette. If no receipt exists, the dialog explains the boundary and offers only
Close/Cancel. It does not create a palette, bootstrap a preset, pick coefficients
or convert the legacy wheel. Dark TonePlan coefficients remain unresolved.

## Edit, Preview, Apply

- C1 uses a **text HEX field**. Its exact six-digit literal spelling/case and
  alpha (including zero) survive; there is no color-input or HSL round trip.
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

**Local browser acceptance has not run.** Default-sandbox Chromium aborted before
creating a page (`socket() failed: Operation not permitted`); one formally
requested escalated run failed before startup with
`bwrap: Can't create file at /home/agent/.codex/sessions: Read-only file system`.
No restriction/flag/executor workaround was attempted. The test is registered
for normal PR CI; DOM success is not visual/browser evidence. Windows Tauri
dialogs, WebView2/native exit/restart, human comfort, final presets/dark
coefficients and ColorF32/Field rendering remain separate acceptance limits.

See [retention and Apply contract](key-color-recipe-retention.md) and
[pure model contract](../../docs/key-color-recipe-contract.md).
