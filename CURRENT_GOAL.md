# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — SVG part node and Windows trial

Goal: Add SVG as a vector part inside the current Composition, repair the attached
SVG import and Layer scroll, add multiple Layer Solo, then distribute Windows
EXE/ZIP promptly. Preserve the current document and completed font/path work.
Authority: user's explicit 2026-10-18 16:37 JST request supersedes the earlier
question-only state. Preserve completed font/path implementation. No main merge,
release, force push, auth changes, font extraction or Notion writes.

Current phase: SVG node, import repair, Layers scroll and multiple Solo are
implemented; integrated production UI passed, including multiple Solo, Hide
priority, saved state, clear/Undo and actual PNG alpha. Combined model/DOM64
and native35 passed; targeted SVG regression also covers transparent no-paint
shapes instead of a default black fill. Synthetic original SVG pixels match exactly, including
stroke, alpha and nonzero holes. The private attached831-rectangle SVG now loads
through both actions; its rounded path approximation has measured mean channel
error0.162/255 at820px raster (edge antialias differences), not exact pixel equality.
The private design is outside Git and excluded from distribution. Existing Import
SVG document remains an undoable replacement; both native entry points use the
Windows SVG chooser/read adapter instead of HTML activation after an async prompt.
Browser input resets on retry; read/parse/refusal/cancel states are visible.
Layers scroll reset was reproduced1282→0; numeric scroll, selection and active
rename now survive Hide, rename confirm/cancel and Undo/Redo. Solo uses the same
top-level filter in canvas/SVG/PNG, supports multiple layers, leaves Hide unchanged,
is saved/undoable at schema33, and has a visible export status. Hide has priority;
Composition Reference source internals ignore this top-level filter as before.

Approach: Reuse the conservative SVG parser, saved editable vector nodes and
Composition Reference. Each SVG imports one hidden source Composition containing
its paths/paints in original order. The current Composition gets a named
SVG · filename instance joined after its existing output. Position, duplication,
paint-aware Transform, Undo and export use existing common Session/evaluation.
No raw SVG execution, external resource fetch, file dependency or new scene model.
The source can be opened for node editing. SVG instance type remains Composition
Reference, rather than introducing a duplicate implementation or saved format.

Next task: Commit, normal push, native build/startup, ZIP/Library. Browser actual
file chooser and desktop JS IPC branch were verified separately. Desktop IO tests
use an explicit stub; native read limit/UTF-8/non-mutation is tested with real files.
Actual native dialog/hands-on editing remains unverified without a UI controller.
Done for next: Confirmed Library ID, hashes, supported subset and actual limits.

Proven prior checkpoint: Path A/direct selection, 1px/Shift10px nudge, Alt independent
handle and both-end Curve/Multi Path Pen continuation are complete at de2b589.
Combined path ZIP is already in Library libfile_fae9c257178481919aa938af765d643a;
SHA256 52D829077B0B46A8544F84FD9304DB9E9102BEBDF60E0BB06E39A368E11CBD7F.
Path model/DOM37, production A/continuation UI and legacy Pen UI passed. New EXE
startup responded without logs; six packaged examples re-export exact SVG offline.
Prior OS wrap schema32, Composition, DIN/axes/vertical work is retained. Additional
OS custom-leading work was investigation only, with no source edits to abandon.

State: codex/desktop-production-20261016, base 16fc21e (delivery record); SVG/Layer
changes are current work. Schema33 is required only when Solo is authored.

Limits: SVG parser's supported paths/rect/circle/ellipse, solid paints, bounded
two-stop local gradients, stroke, fill/stroke alpha and nonzero holes. Unsupported
text/image/use/CSS/transforms/masks/filters/evenodd/nonzero-origin viewports refuse
explicitly. Add requires an unlocked visible regular path Composition Output;
Image Output compositions are not silently rewritten. Existing geometry, depth,
node and paint budgets apply; no silent truncation. Percentage rectangle dimensions
and rounded rectangles are supported. Unnamed opaque same-color rectangles batch
into bounded compound paths; transparent paints, arbitrary paths and explicit
names are not merged. The parent runs the separate read-only specification audit;
do not expand this implementation checkpoint into a full audit.
Native WebView2 IME/high-DPI/
full hands-on editing remains LOCAL_WAIT because the native UI controller is
unavailable; do not repeat inventory without new host evidence. No current code
work depends on it. Full Japanese kinsoku/vertical wrap, OS custom leading/rich/
path/character adjustments and color/bitmap remain explicit font limits.

Double-check: bounded local review against exact reuse/atomicity/appearance and
security contracts. Consultation not-required, zero external sends.
Handoff: CONTINUE_CURRENT_TASK. Finish this explicit SVG distribution priority.
