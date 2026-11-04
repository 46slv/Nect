# Parameter Surface Contract v1.2

Status: bounded production seam on current Desktop main. This document describes the implemented `ParameterSurface` boundary and the remaining migration work; it is not a claim that every Nect parameter or REQ-161 widget family has migrated.

Related product authority:

- [REQ-161 semantic widgets](https://app.notion.com/p/3e0fd279a6f381f4ba45d03c02ebd0fe)
- [R07 Semantic Effect Widget packet](https://app.notion.com/p/3eafd279a6f381d7bf43cd012cba26e3)
- [Interface Method UI Component Contracts](https://app.notion.com/p/409fd279a6f3810c9cc4ee96fbd990c8)

## Goal

An ordinary parameter addition should not require a new global pointer/change branch. An existing parameter should be able to gain another UI without creating another value store, validation path or Undo implementation. A compatible interaction change should be made in one semantic behavior owner and inherited by every bound caller.

This is not one universal widget. It is one parameter contract with composable presentations.

```text
canonical domain definition + authored/effective state + typed graph route
  -> non-persisted ParameterSurface descriptor
  -> context binding / Session transaction
  -> shared semantic behavior
  -> one or more presentations
```

## Six facets

1. **Value meaning** — stable identity, kind, default, unit/subtype, hard validity and semantic traits such as multi-turn angle. The existing product definition remains authoritative.
2. **Authored/effective state** — authored Base, expression/source precedence, lock/read-only and evaluated result. UI stores no second value.
3. **Input/edit policy** — allowed modalities, hard versus soft limits, step/precision/snap, live/staged/async/mixed mode and begin/preview/commit/cancel.
4. **Presentation** — NumericField, Slider, AngleDial, XY/Canvas handle, swatch/channels, Toggle, readout or another typed view. Several may exist simultaneously.
5. **Typed I/O** — accepted source/target types, stable ports/PublicParamID and dependency direction. Graph dataflow is not UI event forwarding.
6. **Authority** — the existing domain owner and Session/draft transaction own validation, persistence and Undo.

The user's simpler model maps directly:

```text
input                  -> input/edit policy
input min/max          -> hard validity + soft adjustment range
input shape            -> presentation(s)
where it receives/sends-> typed I/O
who saves/undoes it    -> authority
```

## Implemented seam

`desktop/src/parameter-surface.js` provides:

- `defineParameterSurface()` — validates and freezes disposable metadata. It contains no authored value, graph edge, document schema or history.
- `parameterPresentation()` / `projectParameterPresentation()` — resolve one declared presentation and its display projection.
- `renderParameterPresentations()` — preflights every projection/adapter, then renders every peer from one binding snapshot. It does not dispatch synthetic events between controls.
- current real descriptors for `document.gridOpacity` and `Geometry Repeater.rotation`.

The first two callers deliberately exercise different semantics:

| Parameter                  | Canonical value              | Presentations                                   | Transaction                                  |
| -------------------------- | ---------------------------- | ----------------------------------------------- | -------------------------------------------- |
| Grid opacity               | ratio `0..1`                 | overlay + percent Slider + percent NumericField | existing `set-guide-setting` Session gesture |
| Geometry Repeater rotation | unbounded multi-turn degrees | exact NumericField + modulo AngleDial           | existing parameter Session gesture           |

Grid opacity proves canonical/display conversion and three synchronized views. Repeater rotation proves `720°` authored can coexist with a `0°` visual dial, driven Base state, exact precision and one gesture/Undo.

## Extension protocol

### Add an ordinary parameter

1. Add or reuse its typed domain definition and stable property identity.
2. Keep hard validity and graph compatibility in that domain owner.
3. Add a non-persisted surface descriptor only when UI/edit metadata is needed.
4. Bind it to the existing Session or staged/async transaction owner.
5. Compose existing presentations and behavior controllers.
6. Add one representative contract/binding test and one real workflow test.

The normal addition must not edit the global pointer/change handler. If it does, first determine whether the missing piece is a binding adapter, semantic family behavior or genuinely new interaction.

### Add a presentation to an existing parameter

1. Reuse the same binding snapshot and transaction owner.
2. Add a presentation declaration and adapter.
3. Keep transient pointer/draft state local to that presentation.
4. Project display values without rewriting authored precision.
5. Refresh every peer from the binding snapshot; never relay `input`, `change` or click events from one UI to another.
6. Verify removal leaves no orphan handler or saved state.

Adding or removing a presentation is UI-only and must not migrate `.nect` documents.

### Add or change shared interaction behavior

Change the nearest semantic behavior owner:

- numeric typing/scrub/keyboard/focus -> numeric behavior owner;
- slider track/keyboard semantics -> Slider family owner;
- angle pointer geometry/wrap indicator -> AngleDial family owner;
- domain validation/connection/evaluation -> domain/graph owner;
- commit/cancel/history -> binding/Session owner.

Do not put all of these into `ParameterSurface`; it is a contract and projection boundary, not a second application core.

### Create a new semantic family

Promote a family only with at least two real callers, repeated drift/bugs, an external/public boundary or an explicit durable product requirement. Prefer, in order:

1. reuse an existing contract;
2. add a binding or presentation variant;
3. extract a shared behavior primitive;
4. converge duplicated metadata toward its existing owner;
5. create a new family.

A family that accumulates caller-specific booleans is no longer generic; split the binding or family.

## Change ownership

| Change                      | Single owner to edit               | Must not require                  |
| --------------------------- | ---------------------------------- | --------------------------------- |
| hard min/max or valid type  | domain definition/validator        | screen-specific validation copies |
| soft range, step, precision | descriptor/edit policy             | persisted schema migration        |
| scrub interaction           | shared numeric behavior            | one patch per parameter           |
| angle display geometry      | AngleDial presentation behavior    | numeric value rewrite             |
| new Slider/dial/readout     | presentation declaration + adapter | second value/Undo path            |
| source/target compatibility | graph/property connection owner    | DOM event coercion                |
| live/staged/async commit    | binding/Session or draft owner     | generic component-owned history   |

“One place” means one owner per invariant, not one file that owns the entire product.

## Acceptance

A compliant vertical demonstrates:

- new ordinary parameter: definition/binding only, no global event branch;
- new presentation: same authored/effective value, permissions, validation and history;
- global compatible behavior change: one semantic owner changes and representative callers inherit it;
- exact display transforms: authored precision survives presentation formatting;
- one changed gesture = one Undo; cancel/no-op = none;
- staged edits remain draft-only until Apply; async edits retain their atomic boundary;
- mixed/locked/source/expression/revision state agrees across presentations;
- invalid descriptor/type or missing adapter fails before partial peer rendering;
- invalid graph connection rejects atomically;
- UI-only changes require no document migration;
- deleting a parameter/presentation leaves no orphan handler or duplicated metadata.

Representative tests, not a cloned full matrix per caller, should cover shared behavior. Binding-specific tests retain product authority, stale target and conversion checks.

## Anti-patterns

- Slider writing a NumericField, or dial dispatching synthetic `change` to another control.
- UI component owning a canonical value or private Undo stack.
- a second persisted `PropertySchema` parallel to normal Parameters/Session.
- screen-specific copies of hard validity or graph compatibility.
- a global registry of hypothetical controls without real callers.
- one universal component with product-specific boolean props.
- rebuilding descriptors or searching the full DOM on every pointer move when stable targets can be captured at gesture start.
- treating a contract document or unit test as GUI/runtime acceptance.

## Current boundary and next migration

The runtime seam currently covers Grid opacity and Geometry Repeater rotation. Other numeric controls still use existing adapters and `numericLimits()` branches. Migrate only when a real vertical benefits, preserving the current domain owner and tests.

Full REQ-161 remains broader: all accepted widget families, generated built-in/Macro parity, save/reopen and typed API/MCP readback. This seam is the reusable implementation path for those later verticals, not their completion receipt.
