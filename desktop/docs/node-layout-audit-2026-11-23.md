# Node layout audit: November 23, 2026

Scope: built-in node types, synthetic documents on Windows Chrome, graph at 100%
with cards set to the 220 px minimum width, and the selected node's Inspector.
Each node was loaded separately. Image and Image Output use the bundled image
sample; SVG uses the regular import command with a synthetic rectangle. This
is a default-state comparison, not a complete check of all modes, connections,
translations, GUI zoom levels, selected-character editing, or native dialogs.
Inspector content height includes the normal Inspector frame.

## Fixes in this checkpoint

- Transform: the nine reposition presets live under a closed-by-default
  Reposition anchor disclosure. Keep the Anchor Point and other actual
  transform parameters visible. Card height 964 → 845 px; Inspector content
  1091 → 982 px.
- Text: the graph card folds Type on Path and Maintain equal ink gaps when
  unused. Active settings are marked in the heading and initially open.
  Content, font, writing direction and parameter sockets keep their existing
  placement. Card height 1371 → 1204 px. Existing Inspector typography
  sections are unchanged.
- Opening, closing and restoring disclosures is view-only; it never edits the
  document or creates an Undo operation. Graph and Inspector state is separate.

## Compact value rows in the luminance family

Common literal numbers and checkboxes are on the label's row. Input/Output
types, attached sources, lock and expression controls, and diagnostics are
unchanged. Connected and expression-driven values keep the full Base/Result
row. Path and Vector2 controls keep their original space.

| Node                         | Card before / after (px) | Inspector before / after (px) |
| ---------------------------- | -----------------------: | ----------------------------: |
| Error Diffusion Dither       |                832 → 736 |                     937 → 731 |
| Error Diffusion Dither Image |                793 → 697 |                     928 → 722 |
| Halftone                     |                832 → 753 |                     937 → 731 |
| Line Screen                  |                530 → 475 |                     657 → 573 |
| Luminance Instances          |                840 → 681 |                     896 → 684 |
| Luminance to Points          |                780 → 661 |                     896 → 690 |
| Ordered Dither               |                832 → 736 |                     937 → 731 |
| Ordered Dither Image         |                793 → 697 |                     928 → 722 |

## Angular Gradient Fill and Vector Transform

Keep color stops, alpha, vector axes, and driven fields in their existing
editors. Put literal scalars and checkboxes on one row with their controls.

| Node                  | Expanded card | Inspector content |
| --------------------- | ------------: | ----------------: |
| Angular Gradient Fill |  817 → 695 px |      966 → 844 px |
| Vector Transform      |  777 → 603 px |     1027 → 841 px |

Same 220 px card fixture in Windows Chrome. The visible compact number inputs
had 60 px and checkboxes 16 px without crossing their field bounds. Linear and
Radial Gradient Fill retain their prior 478 / 627 px layout. The existing
Help panel now keeps its open/closed state while the node re-renders, separately
for graph and Inspector. Notes already inside a disclosure stay there.
Chrome verified Enter toggling with no document change and no effect on the
other surface. Focused Angular Gradient, Vector Transform, Text spacing,
Stroke and Transform tests passed.

## Stroke details

Stroke preset, Width, Color and graph inputs stay visible. The line cap, join
and dash pattern are optional and fold together under Cap, join & dash on the
card and Inspector. At minimum width the card is 645 → 547 px; Inspector content
is 794 → 684 px. Chrome verified opening the section leaves the document
unchanged, and the existing scenario still verifies editing, Undo/Redo,
invalid-value rejection, JSON/SVG/PNG, and dash/cap pixels.

## Text graph: more position references

The flat Text card measured 1,204 px at the clean Draft #347 head. Two 44 px
rows exposed Reference bounds and the cross-axis reference even with the
new-Text defaults (Visible outline and first baseline/column center). Group
those two rows under **More position references** on the graph only, when
unwrapped, detached, and all references are at their defaults. The inline
reference, text, font, size, writing direction, wrap and leading stay visible.
Changing any reference keeps both secondary rows visible, including the legacy
Advance bounds. Position and Path sockets and diagnostics are not in the fold.

| Fixture (graph 100%, Windows Chrome, 1×) | Card before → after | Inspector before → after |
| ---------------------------------------- | ------------------: | -----------------------: |
| 220 px card, 1600 × 1000 window          |      1204 → 1144 px |           1219 → 1219 px |
| 264 px card, 1600 × 1000 window          |      1204 → 1144 px |           1219 → 1219 px |
| 220 px card, 1024 × 768 window           |      1204 → 1144 px |           1219 → 1219 px |

Alternate surfaces checked before choosing the patch:

- Image and Image Output were 311 and 159 px: the asset's placement and size,
  reference, and routing are not an extra group of inactive controls.
- Blend's seven modes were 354–495 px. Inactive parameters are already omitted;
  the additional C or Spine port in the taller modes is part of that mode's
  wiring. Mode, spacing, and Expand remain accessible.
- Wrapped vertical Text (1265 px) keeps the references available for placement
  and resize. Attached Text (1679 px) keeps its existing path controls and
  retained flat references. A custom cross-axis reference (1204 px) stays
  exposed. Saved Windows Text (1174 px) has a separate editor and an existing
  wrap overflow warning; Chrome verified the saved outline UI only, not native
  font preparation. All these fixtures, including their Inspector heights,
  measured the same after the patch.

Chrome verified Enter and Tab, retaining the graph's view choice across
rerenders independently of Inspector typography, the connected Position and
reconnect menu, an exposed maintained-gap suspension warning, and reference
edits with Undo/Redo through a downstream Transform. Toggling alone changes
neither JSON nor evaluated SVG, dirty state, or history. The existing graph
Space-to-pan shortcut is unchanged; use Enter to toggle a focused summary.

## Next cases to inspect and improve

1. Text: remaining typography, reference and wrap controls. Cover Windows
   fonts, attached text and maintained-gap warnings before moving any active
   status. Leave text, font, size and authored results discoverable.
2. Luminance, Halftone and Dither: separate output and sampling controls from
   dot geometry and infrequent controls. Keep connected sockets, source, pitch,
   primary tone, and evaluated warnings visible.
3. Angular Gradient and Vector Transform: check secondary presets and
   modes separately from authoring controls. Never hide active sockets, mode
   switches, locks, warnings, or the only way to reconnect a wire.
4. Recheck narrow and normal widths, keyboard operation, preservation across
   rerender and visibility of active/error states for each modified node.

## Per-node baseline

The table records the initial measurements, before the fixes above.
Image Output and SVG were measured in the second pass; neither was affected.
Height is only a triage signal, not an automatic reason to hide controls.

| Node                         | Card height (px) | Inspector content (px) |
| ---------------------------- | ---------------: | ---------------------: |
| Angular Gradient Fill        |              817 |                    966 |
| Auto Handles                 |              268 |                    417 |
| Bend Points                  |              309 |                    458 |
| Blend                        |              424 |                    481 |
| Boolean                      |              310 |                    388 |
| Circle                       |              346 |                    531 |
| Closed Path Offset           |              379 |                    528 |
| Color                        |              158 |                    408 |
| Combine Vector               |              228 |                    478 |
| Composition Reference        |              237 |                    433 |
| Convert to Path              |              198 |                    347 |
| Corner Round                 |              268 |                    417 |
| Curve                        |              533 |                    683 |
| Dash Path                    |              408 |                    557 |
| Ellipse                      |              346 |                    531 |
| Error Diffusion Dither       |              832 |                    937 |
| Error Diffusion Dither Image |              793 |                    928 |
| Expand Stroke                |              338 |                    487 |
| Expression                   |              158 |                    408 |
| Fill                         |              229 |                    389 |
| Geometry Repeater            |              379 |                    528 |
| Halftone                     |              832 |                    937 |
| Image                        |              311 |                    550 |
| Image Output                 |              159 |                    338 |
| Imported Path                |               88 |                    319 |
| Instance on Points           |              328 |                    406 |
| Join Geometry                |              269 |                    347 |
| Keyer                        |              366 |                    545 |
| Line Screen                  |              530 |                    657 |
| Linear Gradient Fill         |              478 |                    627 |
| Luminance Instances          |              840 |                    896 |
| Luminance to Points          |              780 |                    896 |
| Mask                         |              443 |                    521 |
| Math                         |              269 |                    519 |
| Mirror                       |              368 |                    517 |
| Multi Path                   |              481 |                    671 |
| Null                         |              197 |                    436 |
| Ordered Dither               |              832 |                    937 |
| Ordered Dither Image         |              793 |                    928 |
| Output                       |              159 |                    319 |
| Path Remap                   |              300 |                    428 |
| Polar Transform              |              677 |                    826 |
| Polygon                      |              486 |                    671 |
| Pucker & Bloat               |              327 |                    476 |
| Radial Gradient Fill         |              478 |                    627 |
| Rectangle                    |              346 |                    531 |
| Reference                    |              258 |                    468 |
| Resample Path                |              309 |                    458 |
| Reverse Path                 |              198 |                    347 |
| Roughen                      |              397 |                    546 |
| Separate Vector              |              187 |                    408 |
| Set Closure                  |              257 |                    406 |
| Set Path Start               |              268 |                    417 |
| Simplify Path                |              268 |                    417 |
| Smooth Path                  |              397 |                    546 |
| Star                         |              556 |                    741 |
| Stroke                       |              645 |                    794 |
| Subdivide Path               |              268 |                    417 |
| SVG                          |              237 |                    468 |
| Taper                        |              309 |                    458 |
| Text                         |             1371 |                   1183 |
| Text to Path                 |              159 |                    319 |
| Toggle                       |              147 |                    397 |
| Transform                    |              964 |                   1091 |
| Trim Path                    |              338 |                    487 |
| Twist Points                 |              268 |                    417 |
| Value                        |              158 |                    408 |
| Vector                       |              185 |                    435 |
| Vector Math                  |              269 |                    519 |
| Vector Transform             |              777 |                   1027 |
| Wave Points                  |              508 |                    657 |
