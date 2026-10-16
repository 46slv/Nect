# Desktop source and adoption

Imported from 46slv/Nect-SVG107a154e8d5a887f4bc551ab7bcafbc88773da87 by the predecessor. The retained source-107a154.tar is a local provenance artifact excluded from Git. Imported licenses and lockfiles are retained. Parent CURRENT_GOAL.md owns the adopted Brief and single active checkpoint.

Tauri/WebView2 displays the editor; a DOM-free JS Session will own shared commands, authored graph, stable IDs and Undo. Rust owns file bindings, not a second editor history. Imported direct UI mutations remain an explicit staged migration obligation. Legacy C++/Qt and file formats remain separate preserved references.

Document UI contract: retain the Canvas/Graph/Inspector working surface, expose filename/dirty/saving state and distinct Save / Save As. New/Open/Close share Save, Discard, Cancel. Conflicts and failed writes preserve current edits. Finish/cancel active gesture, Pen or IME before transitions. Normal close is the current target; forced kill/crash recovery is not implied.

External disk checks detect observed byte changes; checking before replacement is not an interprocess compare-and-swap guarantee against noncooperating writers.
