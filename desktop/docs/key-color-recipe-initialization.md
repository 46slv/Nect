# Explicit key-color initialization — developer/test-fixture API only

`initialize-key-color-recipe` is a typed Session command for preparing explicitly
specified experimental fixtures. It is **not ordinary user creation**: there is
no creation button, preset factory, coefficient editor, JSON-import UX or new
`main.js` routing. Product defaults and unresolved dark TonePlan/ChromaPlan
coefficients remain unchanged. See the [pure model contract](../../docs/key-color-recipe-contract.md)
and [historical receipt contract](key-color-recipe-retention.md).

## Closed version-1 request

Use the usual captured `{sessionId, expectedRevision, type, payload}` envelope.
The payload has exactly these fields:

```js
{
  version: 1,                              // Initialization contract version.
  modelVersion: 2,                         // Canonical model snapshot format.
  solverVersion: 'experimental-affine-v1',
  colorSemanticsVersion: 'linear-srgb-straight-v1',
  paletteId: /* explicit stable ID */,
  intent: {
    keyColor: { id, role, value: { hex, alpha }, hueHint },
    preset: {
      id, version: 1, harmony, hueSteps,
      slots: [{
        id, role, position,
        lightnessScale, lightnessOffset, chromaScale, chromaOffset, alpha: 1,
      }],
    },
    slots: [{ id, role, deltaL, deltaRatioCmax }],
    settings: { areaAware },
  },
  allocations: [{ slotId, name }],
}
```

All shown fields are required, including C1 hue hint, settings, each affine
coefficient and each derived plan's opaque alpha. Unknown fields and unsupported
versions reject. Values must satisfy the existing pure model. There is no role,
coefficient, count, name or ID inference; the API selects no product preset.
An empty explicit derived plan/list is supported. Every fresh derived intent ID
must have exactly one active preset mapping: no dormant historic value exists
for a new identity, so an omitted mapping rejects rather than inventing a color.
Existing retain/replace/Apply and dormant snapshot semantics are unchanged.

`allocations` must cover exactly C1 and all derived intent IDs, each once. Order
is authored append order, **not** an index mapping: generated values are matched
only by stable ID. Names are nonblank strings of at most 128 characters, using
the existing shared-slot bound. IDs must be fresh even when an existing name or
color matches; no reuse, remapping or deduplication occurs. The combined old/new
registry must fit the existing 256-slot budget. The global namespace includes
Composition, Layer, Node, Point, Guide, Node-group and node-preset identities,
including entities in inactive Compositions.

The existing native lossless-JSON guard checks the complete raw request before
cloning/generation. Unsupported values/types, nonfinite numbers, accessors,
cycles, sparse arrays, dropped array properties, symbol keys and nonenumerable
properties and nonordinary arrays reject instead of disappearing. The entire guarded
request is then detached before allocation or model methods are consumed; proxies
are not cloneable and reject before any commit. Pure `createKeyColorRecipe` remains
unchanged and produces the canonical revision-zero result. The existing Apply
opaque-derived policy is reused; C1 preserves exact HEX spelling and alpha 0.

## Detached preparation and one commit

`prepareKeyColorRecipeInitialization(document, payload, slotMap)` returns detached
`{snapshot, slots}` proposals without changing artwork, registry or receipt.
`slotMap` is the current registry keyed by stable ID. This helper does not commit
or reserve IDs, and its output is not another authored owner. Preview consumers
may display each proposed new slot's ID, exact name and literal, alongside the
explicit plan and versions. This packet does not add a visual preview interface.

Capture the Session owner and envelope **once before preparation**. Keep the same
explicit request detached from caller edits; check that the current owner is the
captured owner and dispatch that captured envelope, never a freshly read state.
An edit, Undo/Redo, owner replacement or Composition switch invalidates it. Do not
refresh/rebase or substitute an earlier valid preview when later input is bad.
The Session command is the final validation boundary, including entity collisions;
the pure helper neither validates a full project nor reserves its namespace.

The command requires no existing retained receipt. Repeated initialization rejects,
including a request with different fresh IDs; it never implicitly replaces.
It appends proposed slots and the `retained-only` receipt in one private candidate,
raises native schema to 58 and records one History step. Session/project revision
advances once; palette revision stays 0. One Undo restores the previous schema,
slots and absence of receipt; Redo restores the exact generated identities/results.

Composition dispatch performs a narrow proposed-ID check across the full project
**before inner Session commit**. It does not rely on the generic post-validation
fallback, which would rebuild the inner Session. Rejected input preserves authored
state, Session identity/revision, project revision, History and pending Redo.

Existing slot order/names/values, locks, `colorDesign`, graph bindings and references
are untouched. New slots create no bindings. The receipt remains historical:
manual edits do not regenerate or rewrite it, and native save/load validates
without solving. Ordinary New/load still introduces no key-color recipe/default.

## Verification scope

`tests/key-color-recipe-initialization.test.js` parameterizes real Document and
Composition Sessions for exact allocation/C1 alpha 0, atomic Undo/Redo and native
round trips; unrelated legacy preservation; existing-receipt refusal; global
collisions including inactive entities; raw JSON, shape/version/mapping/alpha and
budget rejection with pending Redo; detached preparation and stale envelopes;
and unchanged ordinary New/load. Existing solver, dormant retention, Apply and
editor tests remain the regression authority for their unchanged behavior.

This establishes initialization infrastructure only. Ordinary-user creation still
needs an accepted versioned initial plan and explicit role/count mapping, especially
dark-role lightness ordering, or approval of a particular experimental plan.
