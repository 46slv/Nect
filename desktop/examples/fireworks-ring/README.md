# 花火の輪 / Fireworks Ring

Open [baseline](fireworks-ring-baseline.json) or [variant](fireworks-ring-variant.json)
as an editable Nect document. Matching SVGs are included. Both use the same five
nodes: one owned Curve → Geometry Repeater (ae-2d) → Fill → Stroke → Output.
The 24 diamonds are derived copies, not 24 baked sources.

| Setting                         | Baseline            | Variant                      |
| ------------------------------- | ------------------- | ---------------------------- |
| Canvas / orbit center           | 600×600 / (300,300) | Same                         |
| Copies / rotation step / offset | 24 / 15° / 0        | Same                         |
| Motif-center radius             | 120                 | Same                         |
| Source diamond width / height   | 12 / 28             | Same                         |
| Alternate Scale                 | 100% (neutral)      | 125% on copies 1,3,5…        |
| Fill                            | #F5D76E             | Odd copies #E85D3F           |
| Stroke                          | #39234A, width1.5   | Odd copies #18324A, width1.5 |
| Alternate Fill / Stroke         | Both off            | Both on                      |
| Paint alpha / copy opacity      | 1 / 100%            | Same                         |

The source center and Repeater Anchor are (300,180). Position is
`[120 sin(15°), 120 (1−cos(15°))]`, Scale is `[100,100]`, and Rotation is 15°.
This affine step orbits the canvas center. Alternate Scale acts around the
motif's own center before placement, so the odd motifs grow to 15×35 without
changing their orbit. There are 24 distinct slots at 0°…345°; the last gap closes
the 360° ring without a duplicate copy at 360°.

Select **Diamond 12 × 28** to edit its owned point coordinates/handles. Its
vertices are (300,166), (306,180), (300,194), (294,180), all with zero handles.
Changing a source point parameter updates all 24 copies. The generator verifies
a north-vertex Y edit through Session, all 24 derived updates, and exact Undo.
Both documents retain the same source/node/derived IDs. They use schema 47,
including the preconfigured odd colors even while the baseline switches are off.

Regenerate into a **new absolute directory** (an existing directory is refused):

```sh
node scripts/fireworks-ring-example.js <new-absolute-output-directory>
node scripts/verify-fireworks-ring-render.mjs <that-output-directory>
```

The second command uses the current built production bundle, native CSP,
headless Chrome and Nect's existing PNG export/download UI. It checks exact
Canvas/SVG/PNG pixels and creates a comparison from the two unmodified 600×600
PNGs on identical white panels. Only the common white background and labels
are composed separately. PNGs and renderer screenshots stay outside Git;
the sample source JSON/SVG and these reproducible scripts are retained here.

OS save dialogs, physical input and clean native cold reopen are unverified.
The unrelated Guides DOM failure after Escape remains the separate baseline
maintenance item recorded in [PR61's receipt](../../docs/grid-origin-receipt.md).
This example changes no product code and requires no new EXE.
