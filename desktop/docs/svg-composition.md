# SVG node composition

Add SVG node adds one hidden, editable source Composition per SVG and one
Composition Reference in the chosen Composition. Existing artwork and other
Compositions remain intact. Its addition, including the source, is one Undo.

Inside the source, ordinary imported geometry becomes editable Multi Path
nodes. Consecutive identical paints share an expandable Join Paths followed by
their Fill/Gradient/Stroke nodes and Output. Join keeps independent contour
groups: holes, winding, touching edges and translucent overlaps are painted
individually, rather than unioned into one compound fill. A single source also
uses Join, so additional paths can be connected there.

Different appearances retain the minimum consecutive paint branches; a final
paint-aware Join preserves their SVG order. Equal paints separated by another
paint are never reordered together. Thus mixed-color SVGs are not forced into
one geometry Join and one Fill. Gradient coordinates, stop alpha and stroke
settings stay in their existing paint nodes.

The existing gradient importer represents fill-opacity by multiplying stop
alpha. Its browser interpolation can differ from native SVG rounding; the
compacted graph matches that pre-existing imported appearance exactly.

The existing importer still bounds and batches unnamed opaque rectangles;
separated rectangles already batched into a compound remain one editable
Path with their contours. This does not promise one node per XML element.
Sources above 1,024 vertices retain Imported Path plus Path Remap to avoid
materializing thousands of authored point sockets. This also preserves inputs
above Multi Path's 10,000-vertex limit. Geometry branches retain
the existing 256 independent-group limit and composition appearance budgets.

This construction runs only for new Add SVG node actions. It does not migrate
saved files or change Import SVG document, the renderer, schema, Mask, or node
clipboard semantics. Copying a Reference in the same document shares its source;
it does not create a second independent source Composition.

Verification: `svg-composition.test.js`, `svg-part.test.js`, `svg-part.e2e.js`,
and the existing SVG import/Join/reference/clipboard/mask regression tests.
Browser evidence uses synthetic public SVG only; native desktop dialogs and
the user's running app or recovery data are outside this check.
