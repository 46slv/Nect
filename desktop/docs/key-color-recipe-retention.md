# Retained key-color snapshots — native schema58

This packet adds **opt-in historical persistence only**. It does not select new
coefficients, generate/apply colors, activate a UI or alter defaults. The pure
model is carried from reviewed prototype `535052c4415207143294dea561ad3f96dbdf5456`;
its experimental affine coefficients and editing/alpha policies are unchanged.
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

Still outside this packet: final Tone/Chroma coefficients, current-value takeover,
explicit legacy conversion, generation UI/defaults, live preview/apply gestures,
readability/coverage policy, Neutral/Text restriction, derived-alpha policy,
generic Field evaluation, native GUI acceptance and release.
