# Retained key-color snapshots — native schema58

The packet is **opt-in historical persistence, not a live color driver**. A
separate explicit Session command can materialize its active resolved values once;
loading never does so. No new coefficients, UI or defaults are selected. The pure
model is carried from reviewed prototype `535052c4415207143294dea561ad3f96dbdf5456`;
its experimental affine coefficients and pure editing/alpha policies are unchanged.
See [the prototype contract](../../docs/key-color-recipe-contract.md).

## Separate, versioned storage

The optional project-root field is:

```js
keyColorRecipe: {
  version: 1,                 // Native retention-wrapper version, not solver version.
  mode: 'retained-only',
  committedSnapshot: { /* canonical prototype snapshot version 2 */ },
}
```

Schema58 was allocated after checking main `110073d2e69e5f00bb30dcabe0e8aad9f0459c2a`.
A packet under an older native schema rejects; an unsupported wrapper/mode or
malformed snapshot also rejects. New native writes store canonical prototype v2.
An explicit retain/replace accepts supported prototype-v1 input through the
existing metadata-only normalizer; this never regenerates colors. Native load
requires the canonical stored version and does not upgrade a malformed packet.

The snapshot owns its authored intent and last committed **model** result, not
current registry values or names. They may differ deliberately. A legacy color
edit does not rewrite the historical snapshot, and loading it does not overwrite
current `colorSlots`. This is not live palette integration or Field ownership.
No old `colorDesign.version` is reinterpreted: Absolute Chroma `overrideC`, Neutral
absolute L, role, Lock, alpha and settings retain their original meanings.

## Typed Session operations

Use the existing Session envelope (`sessionId`, exact `expectedRevision`, `type`,
`payload`). There is no UI trigger in this packet.

| Type                                | Payload        | Preconditions / result                                        |
| ----------------------------------- | -------------- | ------------------------------------------------------------- |
| `retain-key-color-recipe-snapshot`  | `{ snapshot }` | Packet must be absent; validates/copies and stores it.        |
| `replace-key-color-recipe-snapshot` | `{ snapshot }` | Packet must exist; explicit replacement, not implicit retain. |
| `clear-key-color-recipe-snapshot`   | `{}`           | Removes only the metadata; absent packet is a no-op.          |

Snapshots must be lossless JSON trees **before** clone/normalization: unsupported
values (including explicit `undefined` on an optional field), nonfinite numbers,
functions/bigints/symbols, cycles/nonplain objects, sparse arrays, dropped array
properties and accessors/nonenumerable properties reject rather than silently
disappearing. The same check protects both Session kinds, native validation and
serialization; it does not change the more permissive pure-prototype input API.

Payload shapes are closed. Retain never silently replaces, and replace never
silently creates. Equality replacement and absent clear preserve revision,
History and pending Redo, including equivalent old-prototype normalization or
object-key ordering (array order is retained). A changed operation uses the existing private candidate
validation and one Session transaction/Undo. Stale session/revision and existing
active-gesture barriers still apply. Invalid input preserves document/project,
colors, references, revision, History and Redo.

Retained input is detached from the caller; ordinary snapshot reads are detached
and History views remain deeply frozen. Clear neither deletes slots nor lowers
schema58. Explicit Undo may of course restore the pre-retain native document.
There is no new slot-delete operation and no legacy set/rename/bind/unbind guard.

## Explicit one-shot application

`apply-key-color-recipe-snapshot` has a closed `{ snapshot }` payload and the same
Session envelope. It validates the complete lossless JSON snapshot and every ID,
then applies **only active rows** (C1 plus the explicit preset's active derived
IDs) to existing shared slots. There is no ID allocation, index retargeting,
renaming, binding change or dormant/unrelated value update. Unlocked existing IDs
can be explicitly selected; fresh IDs are not required. Old `colorDesign`,
Absolute Chroma overrides, Neutral L and all legacy settings remain unchanged.

If **any active target** is in `colorDesign.lockedSlotIds`, the entire command
rejects with `KEY_COLOR_TARGET_LOCKED`, including an otherwise equal/no-op Apply.
It does not silently unlock, skip a target or partially apply other rows. A locked
dormant/non-target row is not a blocker. Existing manual legacy commands keep
their existing behavior; this stricter lock check belongs only to the new Apply.

C1 keeps the snapshot's exact input HEX spelling and alpha, including alpha0.
For this new application boundary, every active derived row must have alpha1,
and an explicit active preset-plan alpha must also be1. Otherwise the whole
command rejects with `KEY_COLOR_DERIVED_ALPHA`, rather than silently changing the
snapshot. Old nonopaque experimental receipts remain valid for retain/replace,
save/load and dormant history; they need an explicitly prepared compatible
candidate before Apply. No pure generator alpha semantics are changed.

Apply copies each stored resolved **`{hex, alpha}`** literal through the existing
shared-color validator. Current rendering therefore uses that 8-bit HEX value;
full-precision linear samples, intent, diagnostics and palette revision remain
unchanged in the receipt. This is not ColorF32/Field rendering (REQ315 remains
separate). Apply does not rerun the solver, choose defaults or certify complete
derived-solver provenance beyond the existing snapshot validator.

Values and receipt are committed in **one private candidate / one History step**;
the first Apply may retain the receipt atomically, and a later one may replace it.
Do not compose public set/retain dispatches or use the current color-slots gesture
to commit values before saving metadata. Equal literals preserve their existing
object order, and equivalent receipt objects use retention's semantic equality:
when both are equal, revision/History/pending Redo stay unchanged. A changed Apply
advances Session/project revision once but copies the supplied palette revision
without incrementing it. Undo/Redo restores values and receipt together.

There is **no sticky ownership or automatic regeneration** after Apply. Manual
set/batch/legacy-wheel edits leave the receipt historical and may move live values
away from it. A UI may compare current values with the receipt, but equality is
not proof of provenance or an active driver. Explicit reapply affects only the
then-active rows; clear releases metadata references without undoing values.

Preview consumers must retain the Session envelope captured at preview start.
Any concurrent edit, Undo/Redo or Composition switch makes that envelope stale;
it must not be silently refreshed at Apply. Pure edits create detached candidates;
invalid pending input must not dispatch an older valid preview, and should disable
Apply until corrected or cancelled. No pending intent, preview or error is stored
in the native packet by this command, and this patch implements no preview UI.

## IDs, dormant references and global ownership

Every snapshot slot ID, including inactive/dormant slots omitted by a later
preset, must exist in the global `colorSlots` registry. IDs are not names or C2/C3
positions; color equality does not identify a slot. Removing a retained ID from a
native file rejects. Clear explicitly releases those metadata references before
any future deletion policy can act; it does not silently prune or retarget them.

The packet exists once at the project root, never on individual Compositions.
Both `CompositionSessionOwner` and the public active-Document projection carry
that global record. Explicit clear propagates through both paths; the public
`applyActiveCompositionDocument` does not resurrect this optional field from the
base project. Its other unknown-root-field merge behavior is unchanged.

## Native IO, foreign import and SVG boundaries

- Native Document and Composition JSON preserve the complete snapshot, including
  alpha, full-precision linear samples, intent, diagnostics, palette revision and
  target/realized ratios. Loading calls validation, not the color solver.
- Legacy documents without the field neither opt in nor advance schema on load
  or save. Existing old-format values/recipes remain byte-equivalent.
- Foreign Composition import keeps the receiving packet and recipe. Only artwork-
  referenced foreign colors are copied/remapped by the existing import policy;
  source packets and unused/dormant source identities are not auto-adopted. The
  existing `Math.max` schema promotion may retain schema58 even without a packet.
- SVG replacement preserves both old `colorDesign` and the new packet alongside
  the existing global registries, deep-copies the metadata and validates the final
  candidate. This also fixes the former omission of old recipe metadata there.
- SVG/PNG are resolved-artwork outputs, **not** authored-recipe interchange. This
  packet does not promise snapshot round-trip through SVG or an active ColorF32
  Field adapter. Export must leave authored metadata unchanged.

## Verification and remaining work

`tests/key-color-recipe-document.test.js` covers both Session kinds: opt-in native
round trips, old Absolute-C/Neutral-L/alpha/ID/ref preservation, explicit lifecycle,
no-op Redo, invalid/stale/gesture rejection, dormant FK/clear, mutable input
isolation, SVG preservation, foreign collisions/no-adoption, root-only ownership
and both clear-projection paths. The unchanged pure-model tests cover deterministic
resolution and both disconnected-gamut regressions.

`tests/key-color-recipe-application.test.js` additionally covers both Session
owners: explicit atomic materialization/replacement, legacy-lock refusal even for
equal targets, active-derived alpha policy, stale/manual edits, no-op Redo, native
round trips, dormant/deactivation/clear preservation, warm bound-color caches,
inactive Composition consumers and invalid candidate/gesture isolation.

Still outside this packet: final Tone/Chroma coefficients, sticky ownership,
explicit legacy conversion, generation UI/defaults, live preview/apply gestures,
readability/coverage policy, Neutral/Text restriction,
generic Field evaluation, native GUI acceptance and release.
