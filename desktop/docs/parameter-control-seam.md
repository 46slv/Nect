# Parameter Control Integration Seam (Nect Desktop)

Checked against Nect main 40088506ef66d1330b084aeafd9b14867fd8948f, 2026-11-03.
Status: source-facing implementation map / **not an implemented or verified REQ-161**.

Normative product authority:
- [REQ-161 Semantic Effect Widget](https://app.notion.com/p/3e0fd279a6f381f4ba45d03c02ebd0fe)
- [R07 Semantic Effect Widget Implementation Packet](https://app.notion.com/p/3eafd279a6f381d7bf43cd012cba26e3), including Parameter Contract v1.1
- [DEC-19 shared property-row interaction](https://app.notion.com/p/3e0fd279a6f3814b8f9edeeded85726a)
- [Current Goal](../../CURRENT_GOAL.md) and [AGENTS](../../AGENTS.md) for active writer, gates and protected invariants.

This file maps the accepted cross-product design to the **existing product architecture**. Do not use it as a second requirement, a saved schema, or a source mutation authorization while C0 has overlapping Desktop/property ownership.

## What a reusable parameter needs

The user-facing idea — input, min/max, control shape, where it receives/sends — becomes six separately owned concerns:

| Concern | Authoritative owner | Should contain |
| --- | --- | --- |
| Definition / value semantics | nearest built-in or accepted Macro parameter definition | stable key/type, default, unit, hard validity, supported typed ports/capabilities |
| Authored/effective state | existing core Parameter / evaluation | base value, expression, lock, source/sourcePort, effective result |
| Edit transaction | Session + product-specific EditBinding | target/revision, live/staged/async, mixed/disabled, begin/preview/commit/cancel, Undo |
| Control descriptor | derived non-persisted projection | soft range, step/precision, allowed editing modes and UI view hints |
| Presentation/interaction | numeric/slider/dial/toggle/color/vector controller | layout, pointer/keyboard/accessible view, no independent saved value |
| Typed source/target interface | existing graph sockets, references and public parameter mapping | typed connection direction, stable node/port/PublicParamID, dependency and precedence |

Graph dataflow connections are **not** UI event bindings. A Slider, NumericField and AngleDial may show one value, but must not synchronize by dispatching events to one another.

The model must preserve the accepted precedence of connected source > saved expression > authored Base where supported. Editing a connected result is read-only unless an explicit Base/driver route is provided; no silent unlink. Saving a UI hint or moving a dial must not rewrite authored 720° to visual modulo 0° or create a native document migration.

## Current code seams

- desktop/src/core.js: existing Parameter type/value/expression/lock/source, node definitions and real validation. node() clones definitions into authored data; UI-only metadata must not be copied into every saved parameter.
- desktop/src/main.js: numericLimits/numericAttributes and rendered control routing. It still maps some ranges, sensitivity and special targets locally. Classify hard domain constraints versus soft editing policy before relocating these mappings.
- desktop/src/numeric-control-behavior.js and numeric-scrub.js: a reusable pending/active scrub gesture that delegates begin/preview/commit/cancel to bindings. Extend through its existing contract instead of making a new parallel Undo stack.
- desktop/docs/parameter-reference-expressions.md: stable numeric reference syntax, authored Base/editable versus Result/read-only, expression/source precedence, composition/identity constraints.
- Repeater Rotation: a real NumericField + circular dial presentation already exists. Dial-specific rendering currently writes a paired input; generalization should render both peers from accepted binding state.
- R07 Macro publication and R02 typed-value ownership: dependencies remain before full built-in/Macro parity can be claimed.

## Proposed extension API boundary (illustrative, NOT a new persisted object)

A product-specific resolver may expose a ControlDescriptor from existing definitions and current editing context:

    identity: nodeId + stable parameter key, optional PublicParamID
    semantics: valueKind + unit + subtype + hard valid domain
    edit: live | staged | async, readOnly/mixed/driven, step/soft range/precision
    views: numeric | slider | angleDial | toggle | enum | color | vector | curve
    io: typed input/output references from existing graph/public port owner
    binding: readAuthored/readEffective + existing semantic command + begin/preview/commit/cancel

The descriptor is temporary and has **no second authored value**. Source and effective values remain product-owned. Dynamic constraints, permission and selection/revision guards must be checked against current state during update and before commit; caching the initial pointer geometry is not authority to use a stale target.

Parameter Definition and Control Descriptor are not required to become separate classes or a global mega-registry. Enrich existing domain definitions or a nearest-type registry only if actual callers need it. Keep each domain's meaning local and let accepted UI controllers consume a compatible read-only projection.

## Bounded implementation path after dependencies unlock

1. **Inventory** real built-in parameters/ports and current handlers. Classify numericLimits entries as hard duplicate, hard missing, soft edit, dynamic, performance guard or presentation only. Do not bulk-move conditions.
2. **Live scalar proof:** reuse existing numeric behavior and Session for one accepted Node parameter. Show exact input plus bounded Slider where meaningful; verify a single transaction, Undo/Redo, cancellation, state sync and stale-target rejection. No new property-specific global pointer-handler branch.
3. **Angle proof:** bind Repeater Rotation numeric input + dial to one value snapshot/transaction. Assert authored 720° remains exact when indicator displays 0°; verify wrap, zero direction, locks/driver/base and native input.
4. **Different commit models:** reuse interaction with one staged Apply draft and one Text mixed-selection case, without silently committing the document. Verify dynamic constraints, revision and partial-failure atomicity.
5. **Typed I/O and Macro:** after R02/R07 prerequisites, qualify connection type/direction/source precedence, stable parameter/public identity, native save/reopen and typed API readback for built-in and Macro exposed properties.
6. **Other semantic views:** Toggle, Enum, Color, Point/Vector and Curve when their real typed value and Macro dependencies exist. Use the R07 real-fixture matrix, not fabricated demo values.

Early vertical acceptance is not REQ-161 Verified. Requirement completion still needs every accepted widget family and built-in/Macro parity per the Notion packet.

## Review checklist for new property or UI

- Does this add a semantic property or merely another view of an existing one?
- Does the current domain definition already own type/hard bounds/port meaning?
- Can the existing behavior controller and Session handle its editing lifecycle?
- Is there one authored value and one transaction owner?
- Do multiple views update through binding state rather than synthetic peer events?
- Are typed connections, driver/base precedence, stable IDs, and save/reopen preserved?
- Does this ordinary addition avoid editing global pointer/change handlers?
- Is the exception justified by a genuinely different edit/connection contract?

A useful change reduces semantic edit points, global-handler fan-out or repeated validation. If it only adds a new abstraction layer without reducing concrete drift, keep the existing local design.
