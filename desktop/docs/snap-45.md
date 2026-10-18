# 45 degree snapping handoff

Base: `42c3a7a9031488b0c2950531be0e40c35e780f1b` on remote
`codex/desktop-production-20261016` (runtime source `d8b8470`).
Candidate branch: `codex/snap-45-20261018`.

The new `src/direction-snapping.js` owns only gesture math. Shared renderer
changes are deliberately supplied as `patches/snap-45-integration.patch` for the
integration owner; they are not applied in this branch's committed `main.js`.
The patch also updates two graph expectations for the newly added automatic
diagonal fallback, and distinguishes a direction guide from stale alignment
guides after graph autopan.

## Inventory and behavior

| Existing operation | Existing snap / constraint | 45 degree behavior |
| --- | --- | --- |
| Final artwork / multi-selection / source generators | XY edges, centers, guides, artboard, grid; Shift H/V | Shift selects nearest H/V/+45/-45 line, held for the gesture; one translation for all sources |
| Text characters | Ink edges/centers/equal gaps, same XY solver | Same displayed-space constraint, then inverse occurrence transform; maintained-gap restrictions retained |
| Null / opted-in Vector / shape or Text position | Same XY solver | Same direction constraint |
| Curve / Multi Path / retained Remap points | Same XY solver | Same constraint, before inverse source transform |
| Existing Bezier handles / Alt-created paired handles | XY endpoint snap / anchor-relative vector | Nearest 45 degree handle angle about the vertex, updated while dragging; preserve independent/coupled mode |
| Pen new vertices / continuation | XY snap on press | 45 degree ray from previous draft vertex, then inverse retained route transform |
| Pen new handles | XY endpoint snap | 45 degree vector from the new anchor; press cursor and snapped anchor kept distinct |
| Multi Path units | XY movement | Same direction constraint and inverse route conversion |
| Transform Anchor | Output-space pivot movement | Same direction constraint, command retains pivot compensation |
| Graph nodes / groups | Edges/centers, 6 CSS px, Ctrl/Cmd bypass, origin clamp | Shift H/V/45; automatic 45 fallback, rigid group motion and direction guide; preserve origin clamp along the ray |
| Transform rotation | Shift absolute 15 degree steps | Already includes every 45 degree multiple, retained unchanged |
| Composition Guides / Layout Grid / ordinary Grid | Axis-aligned scalar target coordinates | All remain eligible for intersection with a constrained diagonal; visible guide denotes an actually reached target |
| Guide drag | One-axis position | Retains one axis; there is no second degree of freedom to constrain |
| Numeric inputs / scrub / object dimensions / scale ray | Exact values or scalar rounding/projection | Retained; these are not XY direction constraints |

Other controls without an existing positional snapping route (for example
gradient control geometry) are not silently advertised as newly supported Snap
targets. No saved diagonal Guide/Grid authoring capability is added. Guide/Grid
schema, overlay visibility policies and source target eligibility are unchanged.

Snap ON additionally attracts to a 45 degree direction through the gesture origin
within **6 CSS pixels**, only when the ordinary XY target search finds no match.
Existing XY candidates win over this fallback. Alt disables canvas target and
automatic direction attraction; Ctrl/Cmd does so for graph targets. Snap OFF
disables automatic canvas attraction; Shift remains an explicit direction
constraint, as it did for H/V. No new settings or document fields are introduced.

For a held diagonal, search uses a per-axis tolerance of `6 / sqrt(2)` CSS px,
so the total correction along the ray stays within six. The nearest explicit
target beats grid; exact ties retain category priority then axis order. A winning
target determines both coordinates; incompatible second-axis guides are removed.
Character gap cross marks and graph guide extents use the final delta.

## Integration and verification

From the repository root after cherry-picking this candidate:

```powershell
git apply --check desktop/patches/snap-45-integration.patch
git apply desktop/patches/snap-45-integration.patch
cd desktop
node --test tests/direction-snapping.test.js tests/axis-drag-dom.test.js tests/axis-drag.test.js tests/graph-node-snap.test.js tests/graph-node-snap-dom.test.js tests/artwork-snap.test.js tests/character-snap-math.test.js tests/transform-handles.test.js
npm run build
# Run Vite preview on 127.0.0.1:5178 in the same host environment as Chrome.
$env:BASE_URL='http://127.0.0.1:5178'
$env:CHROMIUM_PATH='C:/Program Files/Google/Chrome/Application/chrome.exe'
node tests/direction-snapping.e2e.js
```

Focused suite: **45 passed**, including old axis behavior, target priority,
cache behavior, source transforms, cancellation and gesture rollback. New
direction math suite: **13 passed**. Production build succeeded; existing large
bundle warning remains. Production test uses the compiled UI with synthetic
documents, a fresh headless Chrome profile, real pointer/keyboard operations,
and no renderer instrumentation or `/src` browser imports. See the accompanying
verification JSON for successful scenarios; `test-results/snap-45/` retains the
receipt and direction guide screenshot locally.

One unrelated existing test harness limitation was observed:
`artwork-snap-dom.test.js` injects `const evaluate` by replacing the obsolete
import text `evaluate,`, while this base imports `evaluate as evaluateCore` and
already defines `evaluate`. It fails to parse with duplicate `evaluate` before
interaction. It was not counted as passing or changed in this candidate.

Native EXE and physical high-DPI/pen acceptance were not run. No duplicate native
build, release, merge, user window, recovery store, personal document or font
asset operation was performed. The source checkout was never edited. The
integration owner must apply/reconcile the shared renderer patch in its current
branch and run the same focused production checks before native packaging.

Review: codex-review / shared coding review owner, exact module + integration
patch scope. External double-check invoked; status `not-required`, submissions
zero: local source, 45 focused checks and real production operations resolve the
implementation decisions. No external consultation is claimed.
