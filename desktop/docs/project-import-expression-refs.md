# Project import expression references

Fixes the collision failure described in [Issue85](https://github.com/46slv/Nect/issues/85). On maind0d9c926, importing a Value/Circle pair into a project already containing node IDdriver renamed the imported Value to driver-import-1 but retained ref("driver","value") in Circle.radius. The deterministic regression fails on that unchanged source; the noncolliding control passes.

The shared expression remap helper rewrites expression node IDs on copied parameters after allocating collision-safe IDs. Each source layer supplies its own node-ID map: ref() is layer-local, so an outer Composition/layer/point ID, another layer's node, unselected Composition, destination-only or dangling target is not substituted just because its spelling collides. Input projects and destination Compositions stay unchanged.

Only supported Scalar/Vector2/Color expression descriptors in node params, owned point inputs and remap-edit params are considered. Each copied node's matching first argument is replaced. The existing node-parameter keys, authored whitespace, repeated/vector parts, components, operators and socket priority remain unchanged. For a Curve anchor reference of the form `ref("curve-id", "point-id.position", "x")` (or `"y"`), the point ID in the second argument is also replaced only when that exact point belongs to the copied Curve. A missing point key and references to an unselected owner remain unchanged; an ID belonging to a different Curve cannot retarget it. Single-quoted/escaped IDs are not interpreted as new syntax; non-string expressions and unrelated text/name/metadata fields retain their bytes.

Curve anchor references require desktop Document schema 60. A Composition project containing them uses outer `compositionSchemaVersion: 2`, so earlier Document readers reject schema 60 and earlier wrapped-project readers reject the new outer version. New readers retain version 1 for older projects. Applying a new expression resolves the Curve, point, position and component strictly; a previously valid reference whose point or owner was deleted remains authored and is diagnosed as missing. Version numbers remain at their promoted values after clearing an expression, except when Undo restores an earlier snapshot.

The helper is shared by Project Composition import, legacy composition import and node, layer and artwork duplication. Tests exercise the Session routes, Undo/Redo, Curve movement and missing targets, numeric evaluation after collisions, project JSON re-open, frozen inputs and unchanged-reference controls. No filesystem save/reopen, native dialog, rendering pixels or WebView2 UI acceptance is claimed.

Validated JSON inputs may contain shared JavaScript objects without cycles. Import copies each JSON occurrence independently before remapping, so a descriptor alias cannot propagate an internal expression change into metadata or another layer. JSON primitives, including negative zero, and literal own keys remain intact. This adds no accepted input form or parser rule.

From desktop:

```sh
node --test tests/project-import-expression-refs.test.js tests/composition-project-import.test.js tests/project-composition-import.test.js tests/parameter-reference-expressions.test.js tests/composition-session-owner.test.js
node --test tests/*.test.js
node node_modules/prettier/bin/prettier.cjs --check .
node scripts/check-native-build-config.mjs
node node_modules/vite/bin/vite.js build
```

Identity rewriting runs while preparing an explicit duplication or import. It adds no file/network/IPC operation, dependency, background work, provider/updater behavior or native permission. PR84/86/87 remain separate; no main integration, release, installer or user-session action is included.
