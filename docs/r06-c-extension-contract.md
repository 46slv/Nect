# R06-C — operator registry entry contract and coverage qualification

Status: R06-C1 built-in descriptor consolidation implemented and verified; external extension registry remains DESIGN_GATE, 2026-09-27. Baseline: `codex/practical-alpha@dbdf7a8001ea21adb08048a6b489ccab206e8dbe`. Source authority: [Completion Route R06](https://app.notion.com/p/3e3fd279a6f381b4ba9dd2d1bf066e0f), [P06](https://app.notion.com/p/3e3fd279a6f381ee8ab0ea5ea614c006), Confirmed [REQ-68](https://app.notion.com/p/3e0fd279a6f38145aa6be732b1ed863d) and [REQ-69](https://app.notion.com/p/3e0fd279a6f38164ac96c1f4cda26a5d). This packet records the bounded design input; it does not authorize a new executable plugin loader, external API/dependency, or save-format redesign.

## Verified current boundary

At baseline, the native `ShapeOperation` stores stable instance ID, type, behavior version, enabled state and Scalar parameters. `default_operation` in `src/shape.cpp` knew Fill, Stroke, Repeater, Offset and Group Posterize. `operator_types` in `src/io.cpp` separately enumerated those five types. The Effects catalog in `src/desktop/window.cpp` separately listed only Offset and Group Posterize. Core validation and `src/desktop/canvas.cpp` / `src/shape.cpp` dispatched by type strings. These were working built-ins, but the separate lists were **not an extension registry**. A new type could not be added with its metadata alone, and there was no third-party loader or executable extension boundary.

`op.<instance>.<parameter>` already enters the shared Scalar property/Session path. Group Posterize `levels` is visible in Properties, API, native and the existing expression/pick-whip machinery for numeric properties. REQ-69 additionally requires plugin parameters through Preset, Macro and MCP; those consumers are not implemented as a shared extension route. JSON-lines `--serve` is not MCP. Non-Scalar extension parameter types require the R02 typed-property contract; forcing them into doubles would change their meaning.

## Candidate registry contract to review before implementation

1. One immutable type descriptor binds stable namespaced `type_id`, positive `behavior_version`, applicable target scope, explicit input/output domain, ordered parameter definitions (stable key, type, unit, default and validation), an evaluator implementation, and export support/loss policy. A type is selectable only when all required execution and validation callbacks are present. Metadata-only registration must not produce a selectable fake effect.
2. The same descriptor supplies API discovery and Effects catalog labels; Session validates target, parameter schema and version before mutation. A registered type's parameters use stable instance refs and the ordinary authored-property, revision, Undo, link/expression and serialization paths for their actual types. Bypass and reorder retain the existing command semantics.
3. Unknown or unsupported type/version never executes and never silently substitutes another type. On read/import failure the original file remains untouched; the error names type, version and instance. Whether a document can open in a read-only opaque missing-extension state, and how its exact unknown bytes survive save, is a separate native-source contract. Do not write a partial fallback document while that contract is open.
4. Registration and lifecycle must define namespace ownership, duplicate type/version refusal, deterministic enumeration, registration freeze before document decode, thread and process ownership, failure containment, version migration, and how the source file finds its extension again. These are prerequisites to loading executable third-party code. Loading policy, ABI/packaging, trust and permissions remain an explicit design/authority gate.
5. A first executable acceptance slice needs a real additional operator registered through the proposed route, with a built-in and an extension implementation exercising the **same** discovery, Session, native reopen, UI apply/edit, evaluator and refusal tests. Merely moving the five hardcoded strings into a table is preparation, not REQ-68 proof. An in-process test registration can prove dispatch mechanics but cannot prove install/discovery of a third-party package.

### Positive and negative oracles for the next implementation packet

- Register a test operator on a declared domain, discover its stable type/version/schema in API and panel, author one instance in each route, edit a parameter through normal property controls, and compare exact authored state and output. Close/reopen native in another process with the same registered extension and verify instance IDs, parameter values and output.
- Reject duplicate namespace/type/version, unsupported target/domain, invalid/noninteger parameter, stale revision, missing implementation and version mismatch without changing Document/revision/history. With the extension unavailable, refuse execution with named type/version/instance and preserve the original file. An unknown type must not be treated as Offset or Posterize.
- Confirm that a disabled instance preserves its input and that SVG/export planning reports support or loss from the descriptor. Prove another parameter type only after its normal typed-property contract exists.

## AE coverage comparison

This is a category-level gap table against Adobe's [After Effects effect list, updated 2026-05-13](https://helpx.adobe.com/after-effects/desktop/apply-effects-and-animation-presets/effects-and-animation-presets/effect-list.html). Names in one row do not imply behavioral or bit-depth parity. Nect is currently a static image tool. Its own vector [Offset Paths](https://helpx.adobe.com/after-effects/desktop/drawing-painting-and-paths/shapes-and-shape-attributes/use-offset-paths.html) analogue is separate from AE's raster Distort > Offset effect.

| AE category / feature family | Current Nect source-backed coverage | Classification / gap |
| --- | --- | --- |
| Shape layer paint and path operations | Fill, Stroke, Repeater and Object-local Offset operate on Nect editable shape sources. | Bounded native analogues; no general AE shape-operation parity. |
| Stylize | Group Posterize v1 quantizes 8-bit sRGB postchildren pixels at 2–16 levels. | Bounded native analogue to AE Posterize; no matching claim for AE parameter range, color depth, alpha or layer semantics. Other Stylize effects unsupported. |
| Color Correction; Blur & Sharpen; Noise & Grain | No registered operator in these AE effect families. | Unsupported. Existing paint/color controls do not count as the effects. |
| Distort; Perspective; 3D Channel; Simulation | Nect has 2D transforms and source-local Offset, but no corresponding AE raster/3D/simulation effect. | Unsupported as AE effects. |
| Channel; Keying; Matte | Nect has explicit blend modes and geometry masks as separate composition features. | Alternative primitives only; no AE Channel/Keying/Matte effect compatibility. |
| Generate; Text; Expression Controls | Nect has editable primitives/Text and numeric property expressions/links. | Different authoring paths; no AE effect/control compatibility claim. |
| Audio; Time; Transition; immersive video; utility | No temporal/audio/immersive effect evaluator. | Unsupported in the current static-image scope. |

The table is a planning inventory, not an acceptance test for AE import or equivalent rendering. New categories need exact effect/version/domain/bit-depth oracles before claiming coverage.

## R06-C1 implementation and evidence

`BuiltinOperationType` now supplies the five built-ins' type, label, target kind, input/output domain, behavior version, default Scalar parameters and Effects-catalog visibility. `default_operation`, `operator_types` and the Effects catalog consume that same immutable definition; normal execution and validation remain in their existing owners. The UI test cross-checks the real panel rows against API-discovered type/version/domain. This removes three points of metadata drift without claiming dynamic registration. Native 0.25 bytes and the evaluated algorithms did not change.

Release full build passed, followed by serial `ctest --test-dir build -C Release --output-on-failure -j1` **42/42 PASS**. That includes `effects_panel_interaction`, `compositing_contract`, `compositing_canvas_contract`, `process_contract` and `mcp_desktop_contract`. No hands-on GUI acceptance was performed. The test proves catalog/API agreement for built-ins, not third-party installation, code loading, missing-plugin recovery, Preset/Macro/MCP parameters, or REQ-68/69 closure.

## Gate and next route

The current Mission can continue a bounded built-in descriptor consolidation once an actual caller and acceptance are frozen. Full REQ-68 requires an executable external-extension installation/discovery contract, including code trust and missing-extension native handling; that is a local DESIGN_GATE rather than a reason to pause independent R03/R04/R09 work. REQ-69 remains open until extension-owned non-Scalar parameters and Preset/Macro/MCP consumers exist and pass the same-property route. R06/P06 and L2 are open.
