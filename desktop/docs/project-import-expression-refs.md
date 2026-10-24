# Project import expression references

Fixes the collision failure described in [Issue85](https://github.com/46slv/Nect/issues/85). On maind0d9c926, importing a Value/Circle pair into a project already containing node IDdriver renamed the imported Value to driver-import-1 but retained ref("driver","value") in Circle.radius. The deterministic regression fails on that unchanged source; the noncolliding control passes.

The shared import remap helper now rewrites expression node IDs on copied parameters after allocating collision-safe IDs. Each source layer supplies its own imported node-ID map: ref() is layer-local, so an outer Composition/layer/point ID, another layer's node, unselected Composition, destination-only or dangling target is not substituted just because its spelling collides. Input projects and destination Compositions stay unchanged.

Only supported Scalar/Vector2/Color expression descriptors in node params, owned point inputs and remap-edit params are considered. The quoted-ID matcher follows the existing duplicateNodes rule in core.js. Every matching first argument is replaced; authored whitespace, repeated/vector parts, parameter keys, components, operators and socket priority are retained. Single-quoted/escaped IDs are not interpreted as new syntax; non-string expressions and unrelated text/name/metadata fields retain their bytes. This is identity remapping, not parsing/evaluation or a new syntax/admission rule. Core parser, duplicate behavior, schema, validators and UI/lifecycle are unchanged.

The helper is shared by Project Composition import and legacy composition import. Tests exercise both actual Session routes and one Undo/Redo transaction, numeric evaluation after collision, project JSON re-open, frozen inputs, collision suffixes and unchanged-reference controls. No filesystem save/reopen, native dialog, rendering pixels or WebView2 UI acceptance is claimed.

Validated JSON inputs may contain shared JavaScript objects without cycles. Import copies each JSON occurrence independently before remapping, so a descriptor alias cannot propagate an internal expression change into metadata or another layer. JSON primitives, including negative zero, and literal own keys remain intact. This adds no accepted input form or parser rule.

From desktop:

```sh
node --test tests/project-import-expression-refs.test.js tests/composition-project-import.test.js tests/project-composition-import.test.js tests/parameter-reference-expressions.test.js tests/composition-session-owner.test.js
node --test tests/*.test.js
node node_modules/prettier/bin/prettier.cjs --check .
node scripts/check-native-build-config.mjs
node node_modules/vite/bin/vite.js build
```

The change runs only while preparing an explicit import. It adds no file/network/IPC operation, dependency, background work, provider/updater behavior or native permission. PR84/86/87 remain separate; no main integration, release, installer or user-session action is included.
