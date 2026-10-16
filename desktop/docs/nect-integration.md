# Desktop source and adoption

Imported from 46slv/Nect-SVG107a154e8d5a887f4bc551ab7bcafbc88773da87 by the predecessor. The retained source-107a154.tar is a local provenance artifact excluded from Git. Imported licenses and lockfiles are retained. Parent CURRENT_GOAL.md owns the adopted Brief and single active checkpoint.

Tauri/WebView2 displays the editor; a DOM-free JS Session will own shared commands, authored graph, stable IDs and Undo. Rust owns file bindings, not a second editor history. Imported direct UI mutations remain an explicit staged migration obligation. Legacy C++/Qt and file formats remain separate preserved references.

Document UI contract: retain the Canvas/Graph/Inspector working surface, expose filename/dirty/saving state and distinct Save / Save As. New/Open/Close share Save, Discard, Cancel. Conflicts and failed writes preserve current edits. Finish/cancel active gesture, Pen or IME before transitions. Normal close is the current target; forced kill/crash recovery is not implied.

External disk checks detect observed byte changes; checking before replacement is not an interprocess compare-and-swap guarantee against noncooperating writers.

## Command migration, stage 1

`core.js` CommandSession owns one History. Its typed connect/disconnect/set-operation commands use private candidates, shared validation and Closed Offset/Text Path protection. Envelopes bind Session identity and monotonic History revision; stale/foreign requests and active gestures refuse before mutation. Detached snapshots, Undo/Redo and `window.nect.dispatch` expose the same state. GUI socket dropdowns/dialogs, wire drop/disconnect, context disconnect and operation controls now call this dispatcher. Creation-time internal linking remains part of the legacy creation transaction.

This is an intermediate migration. `legacyHistory` exists only to bridge remaining GUI callbacks, and the old `window.nect.document` debug adapter still returns live state for compatibility. Properties/text/assets/artwork/layers/gestures and the debug adapter must migrate before claiming the whole shared Session contract. Do not expose the legacy bridge as the production API or call the full migration complete.

Evidence: command-session.test.js proves fallback/stable IDs, shared Undo/Redo, no-op revisions, foreign/stale/cycle/operation/lock refusal and gesture exclusion. command-session.e2e.js compares a real GUI operation control against the same headless command, then checks shared UI Undo/API Redo/stale refusal. Existing wiring.e2e.js exercises real browser wire interaction and one-step history. Relevant command/core/gradient tests25/25 and browser parity/wiring passed. This does not establish native IME or all remaining UI command routes.
