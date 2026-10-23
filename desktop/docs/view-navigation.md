# View navigation

## UI research and bounded decision

The existing desktop-first white/silver layout remains: upper artboard, resizable lower node graph, Layers/Inspector at right. Prior 1280×800 and 1600×1000 screenshots showed an artboard labeled “100%” whose actual CSS scale was smaller, a clipped graph with only an origin-reset “Home” control, and long expanded cards whose headers could leave the viewport. The code also restored a non-scrolling Inspector inner element rather than the scrolling sidebar.

Patterns were checked against primary product documentation:

- [Figma zoom/view options](https://help.figma.com/hc/en-us/articles/360041065034-Adjust-your-zoom-and-view-options): zoom, fit and selection framing.
- [Adobe Illustrator viewing artwork](https://helpx.adobe.com/illustrator/using/viewing-artwork.html): explicit actual-size versus fit and pan.
- [Blender node introduction](https://docs.blender.org/manual/en/4.4/interface/controls/nodes/introduction.html): node-editor pan, Frame All and Frame Selected. Direct retrieval was unavailable; official search excerpts supported these navigation patterns.
- [DaVinci Resolve 20 Fusion Visual Effects training](https://documents.blackmagicdesign.com/UserManuals/DaVinci-Resolve-20-Fusion-Visual-Effects.pdf?_v=1757574012000), pp. 24–25: context-specific view zoom, pan, actual size and fit.

These are adapted interaction patterns, not a compatibility claim or a broad redesign. No panel zoom, permanent Hand tool, node rearrangement or canvas-selection fitting is added. Canvas selection fitting remains deferred until bounds for selected source outputs, transforms and character instances are consistently verified.

## Artboard

- − / % / + displays real scale: 100% is one SVG document unit per CSS pixel, independent of device pixel ratio.
- **Fit artboard** centers the entire artboard with 35 CSS px available margin; it recomputes when the viewport/splitter changes. **100%** sets actual size.
- Manual zoom preserves its actual scale and the world coordinate at the viewport center on resize. Buttons zoom about that center; Ctrl/Cmd+wheel zooms about the pointer.
- Plain wheel/two-finger motion pans both axes. Shift with a vertical wheel pans horizontally. Space+left drag and middle drag remain available over artwork, guides and Pen mode.
- Scale limits are 0.001–800%, including very large valid artboards. Fit and manual limits agree; zoom-out never increases scale. The percentage shows additional decimals below 1%.

## Node graph

- − / % / +, **Fit all**, **Fit selection**, and **Reset view** are visible. Fit selection is disabled when no node is selected.
- Actual card width/height and 8 world-px port allowance define the fitted union. The 2400×1600 scrolling floor is not treated as content. Fit requests 32 screen-px padding, subject to the existing nonnegative scroll origin.
- Limits are 5–200%. A fit below 5% reports that some nodes may remain outside the view instead of pretending to contain the graph. Reset view means 100% at scroll origin, while Ctrl/Cmd+1 preserves the center anchor.
- Space, middle or Ctrl/Cmd+Shift drag pans. Plain wheel remains native two-axis scrolling. Ctrl/Cmd+wheel zooms about the pointer, normalizing pixel/line/page delta modes.
- Existing edge-autopan, expandable right/bottom bounds, nonnegative positions and one-gesture undo remain unchanged. Extra scroll extent required for centering survives unrelated rerenders and card disclosure changes.

### Autopan update acceptance

- While dragging a node or wire, unchanged pointer/modifier input and unchanged actual scroll position must not cause repeated gesture-preview, graph-bounds or wire work on animation frames. This includes a nonzero edge velocity clamped at the top/left origin and the first frame with zero elapsed time. Edge checks continue so later motion can still scroll.
- When the viewport actually scrolls, the same frame updates the node preview/snapping or wire preview against the current world pointer, even if the pointer is stationary at the edge. Pointer and modifier changes retain synchronous feedback.
- Preview remains unauthored until commit. Ordinary node/group drag, snapping and bypass, connection/disconnection, cancellation and Undo/Redo keep their existing contracts; one completed changed gesture remains one Undo entry.

`tests/graph-autopan-work-dom.test.js` supplies controlled frame/scroll geometry and
checks these source-level update counts together with gesture/history behavior.
The focused source test group passes 29/29; [call-count evidence and limits](graph-autopan-work.md)
record the baseline comparison and the separate existing overview test failure.
These tests do not measure live UI frame times or establish perceived speed on user
documents. Installed Windows EXE behavior is unverified, and no EXE was built or
distributed for this change. The update rule is automatic interaction behavior.

## Focus and editing safety

Focus a viewport or its view controls for +/−, Ctrl/Cmd+1 actual size, Shift+1 fit, and Shift+2 graph Fit selection. Pointer-clicking a view control returns focus to its viewport; keyboard activation leaves the control focused. Space on a focused button keeps its native action. Viewports have a visible focus outline, concise gesture hints and grab/grabbing states.

Inputs, textareas, selects, contenteditable descendants, floating character controls, Guides and dialogs retain their input behavior. Navigation does not rebuild the document UI, discard drafts/carets, change selection, add undo entries, mark the document dirty or alter SVG/PNG export dimensions. IME composition and active node/object/resize/wire/scrub gestures block competing navigation. Busy graph gestures also block wheel-driven native scrolling beneath the captured pointer. Ordinary document rerenders preserve the actual sidebar scroll and focused viewport.

## Verification

Pure geometry and synthetic DOM tests cover fit bounds/limits, anchor equations, wheel normalization, scoped shortcuts, exempt controls, composition, busy navigation, unchanged document/history/SVG, drafts and sidebar/focus restoration. `tests/view-navigation.e2e.js` adds real Chrome checks for actual CTM percentage/100%, cursor/center anchors, independent views, fit card height, resize, both 1280×800 and 1600×1000 layouts, real wheel gestures, sidebar editing continuity, busy scrub scroll and sub-1% zoom direction. Existing navigation/editing regression suites remain part of the full Chrome command.

Local Chromium cannot launch because its socket operation is restricted. New Chrome checks and screenshots must be verified on the exact Windows CI commit before a browser pass is claimed. Native WebView2, physical high-DPI devices, trackpad pinch and real Japanese IME are separate unverified checks; synthetic composition events are not native IME evidence.

## Graph node alignment snapping

Dragging graph card headers attracts their left/center/right and top/center/bottom
anchors to other cards within **6 CSS screen pixels**, at any graph zoom. The
nearest valid match wins independently per axis; stable node IDs break ties.
Current measured card dimensions include expanded controls and custom widths.
Selected cards move as one group, retain their relative spacing, and never snap
to each other. The group cannot cross the graph's top/left origin.

Dashed blue guides show only the alignment actually applied. Hold **Ctrl/Cmd**
during a node drag to bypass snapping, including without moving the pointer;
release to restore attraction. Ctrl/Cmd+Shift at pointer-down still pans, Shift
click still selects, and Alt behavior is unchanged. This is graph-card layout,
not artwork snapping or automatic graph arrangement. Locked compositions remain
read-only; parameter locks continue to protect parameter values, not card layout.

Autoscroll recomputes snapping from the current world pointer. Pointer-up commits
one ordinary graph-translate gesture; Undo/Redo includes the snapped position.
Escape and pointer cancellation discard the preview and guides. Guides are only
DOM feedback and are never saved/exported as document metadata.

Focused coverage: `tests/graph-node-snap.test.js` and
`tests/graph-node-snap-dom.test.js`, using synthetic DOM events, explicit zoom
and scroll geometry, the starter graph and shipped `examples/boolean-remap.json`.
These checks are source-bound behavior evidence, not native pointer or rendered
browser visual acceptance; real WebView2 guide appearance remains unverified.

## Optional graph overview

The rightmost **Overview** button opens/closes a small overlay inside the node graph.
It starts closed; its visibility is session-only view state, with no new stored settings
or document schema. Existing pan, zoom, fit, Find and native scrollbars remain.

The map shows measured card rectangles and the current viewport, including graph zoom
and the graph's 16-unit margin. It fits content, the visible region and the origin with
padding instead of scaling the 2400×1600 background floor. Resize, card expansion,
node movement, zoom and scroll update the map. Narrow or short panels shrink the overlay.
Wires and card contents are intentionally omitted.

Click outside the viewport rectangle to center there, or drag inside it without an
initial jump. The map projection stays fixed during the drag and updates on release.
Only scrolling changes; node coordinates, selection, connections, Output, document
revision/dirty state, history and export remain unchanged. Arrow keys while the map
is focused pan 80 CSS pixels. Escape during a drag restores its starting view;
Escape while idle closes the map and returns focus to the toggle. Pointer cancellation,
lost capture, window blur, hidden document and structural rerender also stop the drag.
IME, another active gesture, rename, Pen draft and open dialogs prevent map dragging.
Map wheel zoom is not added. Scrolling reuses card geometry and changes the viewport
rectangle rather than replacing every card rectangle.

`tests/graph-overview-dom.test.js` exercises the actual event handlers with synthetic
DOM events and explicit measured geometry: empty/single/large graphs, node and viewport
size changes, open/close, click/drag, frozen projection, competing pointers/gestures,
zoom, scroll, keyboard, cancellation, and unchanged document/SVG/history/file state.
The geometry is supplied by the test: these are not rendered-browser, native touch,
WebView2, high-DPI or visual acceptance results. The pre-existing
`view-navigation-dom.test.js` has an unresolved relative import in its data-URL harness;
that older suite is not claimed as passed. Browser execution remains blocked by the
previously established sandbox restriction; no alternate launch route was attempted.
