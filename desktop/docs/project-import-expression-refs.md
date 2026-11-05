# Project import expression references

Fixes the collision failure described in [Issue85](https://github.com/46slv/Nect/issues/85). On maind0d9c926, importing a Value/Circle pair into a project already containing node IDdriver renamed the imported Value to driver-import-1 but retained ref("driver","value") in Circle.radius. The deterministic regression fails on that unchanged source; the noncolliding control passes.

The shared expression remap helper rewrites expression node IDs on copied parameters after allocating collision-safe IDs. Each source layer supplies its own node-ID map: ref() is layer-local, so an outer Composition/layer/point ID, another layer's node, unselected Composition, destination-only or dangling target is not substituted just because its spelling collides. Input projects and destination Compositions stay unchanged.

Only supported Scalar/Vector2/Color expression descriptors in node params, owned point inputs and remap-edit params are considered. Each copied node's matching first argument is replaced. The existing node-parameter keys, authored whitespace, repeated/vector parts, components, operators and socket priority remain unchanged. For a Curve anchor reference of the form `ref("curve-id", "point-id.position", "x")` (or `"y"`), the point ID in the second argument is also replaced only when that exact point belongs to the copied Curve. A missing point key and references to an unselected owner remain unchanged; an ID belonging to a different Curve cannot retarget it. Single-quoted/escaped IDs are not interpreted as new syntax; non-string expressions and unrelated text/name/metadata fields retain their bytes.

Curve anchor references require desktop Document schema 60. A Composition project containing them uses outer `compositionSchemaVersion: 2`, so earlier Document readers reject schema 60 and earlier wrapped-project readers reject the new outer version. New readers retain version 1 for older projects. Applying a new expression resolves the Curve, point, position and component strictly; a previously valid reference whose point or owner was deleted remains authored and is diagnosed as missing. Version numbers remain at their promoted values after clearing an expression, except when Undo restores an earlier snapshot.

Curve incoming and outgoing handle references use `point-id.handleA` and `point-id.handleB` with `x`, `y`, or `length` as the third argument. They require Document schema 61 and retain wrapper version 2. A reader whose embedded Document limit is 60 must reject the wrapper instead of accepting the unknown semantics. Imports remap a handle point only with its owning copied Curve, retain the field and projection, and promote according to selected references. A schema 60 source's dangling handle-shaped x/y expression and a two-argument Color reference keep their original unresolved state and do not themselves promote an import.

The schema 60 wrapper codec historically accepted opaque handle-shaped expressions even when their Curve owner was present; the Document Session rejected those expressions. The wrapper still round-trips that text, and the Document Session still refuses to interpret it under schema 60. If such a selected legacy Composition is imported into an existing schema 61 project, import rejects atomically rather than assigning the old text a new meaning. A legacy `x`/`y` reference whose owner is missing remains importable and missing. Legacy handle-shaped keys are not point-remapped during import.

The `length` component was not valid in schema 60, even for a missing owner. Selected Composition imports check every retained parameter descriptor, including connected Path, PaintedPath, Image and Boolean inputs, against the source schema before destination promotion. Handle `length` expressions require numeric or Color parameters; they cannot be stored on unsupported input types. Existing missing-owner `x`/`y` and two-argument Color references are unaffected. Unsupported expressions that neither parser can interpret remain opaque.

Older projects could retain an arbitrary parameter key after deleting its target node, including a key ending in `.position`. That legacy missing-node state remains readable; the key alone cannot prove that the deleted owner was a Curve. A present Curve with a point-position key requires schema 60. When importing a schema 60 source with a missing owner and an `x`/`y` point-shaped reference, the ambiguity is preserved conservatively with schema 60. The Composition codec rejects unknown future Document versions instead of accepting them as version 2.

Applying a detached node preset or pasting copied nodes rejects atomically if a previously missing point-shaped reference would acquire a matching Curve and point in the destination. A known external Curve reference copied within its original layer remains on that source; a still-missing reference remains missing. Preset and clipboard source data are retained on refusal.

The helper is shared by Project Composition import, legacy composition import and node, layer and artwork duplication. Tests exercise the Session routes, Undo/Redo, Curve movement and missing targets, numeric evaluation after collisions, project JSON re-open, frozen inputs and unchanged-reference controls. No filesystem save/reopen, native dialog, rendering pixels or WebView2 UI acceptance is claimed.

Validated JSON inputs may contain shared JavaScript objects without cycles. Import copies each JSON occurrence independently before remapping, so a descriptor alias cannot propagate an internal expression change into metadata or another layer. JSON primitives, including negative zero, and literal own keys remain intact. This adds no accepted input form or parser rule.

From desktop:

```sh
node --test tests/project-import-expression-refs.test.js tests/composition-project-import.test.js tests/project-composition-import.test.js tests/parameter-reference-expressions.test.js tests/composition-session-owner.test.js
node tests/curve-handle-legacy-reader-check.mjs
node --test tests/*.test.js
node node_modules/prettier/bin/prettier.cjs --check .
node scripts/check-native-build-config.mjs
node node_modules/vite/bin/vite.js build
```

Identity rewriting runs while preparing an explicit duplication or import. It adds no file/network/IPC operation, dependency, background work, provider/updater behavior or native permission. PR84/86/87 remain separate; no main integration, release, installer or user-session action is included.
