# Keyer + Mask editable example receipt

Separate sample/documentation scope from PR69
`48ed974d108d863ff17a888f742529b6f04acb95`. No product source, evaluator,
schema, native binary or PR69 artifact changes. The outdated no-eyedropper note
in `image-effects.md` now links to the [short guide](keyer-mask-example.md).

The existing green/red/alpha fixture in `tests/image-effects.e2e.js` is reused.
No saved Mask example existed under `examples/`. Two documents retain the same
IDs, five nodes and four connections:
`Image -> Keyer -> Mask -> Output`, `Rectangle -> Mask.Shape`.
The initial Key color is magenta; one Pick from input click on green produces
the finished document exactly. Source PNG stays embedded and editable; the mask
is a separate Rectangle, center[40,40], size[56,56] on an80x80 canvas.

## Actual checks

- `scripts/keyer-mask-example.js`: expected pre-Keyer RGB and keyed alpha,
  one pick/Undo/Redo, mask-size edit/Undo, stable IDs/source assets, closed mask
  and exact JSON/SVG roundtrip passed. Generated files use Nect's serializer.
- `scripts/verify-keyer-mask-example.mjs`: headless existing Edge, production
  Web bundle/CSP and web PNG download route passed. The script asserts that
  Tauri internals are absent before any export. No new native GUI process,
  OS dialogs or interaction with the pending test process/profile.
- Actual headless Pick from input produces the expected finished JSON exactly
  and supports one Undo/Redo. Both documents reload with exact SVG output.
  Before/after PNGs have full RGBA equality with their generated SVGs.
  Invert correctly swaps inside/outside. No page errors or external requests.
- The three bundle assets match sealed PR69 product source
  `7f9817b1bde34c49958905cbe5acd04b4c28f0e9`; no new build was needed.
  The two PNGs and headless editor screenshot were visually inspected.

| Pixel                              | Before alpha | Finished alpha |
| ---------------------------------- | ------------ | -------------- |
| Green inside mask [20,20]          | 255          | 0              |
| Upper red inside [50,30]           | 128          | 128            |
| Lower red inside [50,60]           | 255          | 255            |
| Outside rectangle [74,60] / [6,20] | 0            | 0              |

The verified documents/SVGs copied into `examples/keyer-mask/` match the D output
byte-for-byte. JSON formatting is the canonical serializer output. PNGs and the
screenshot remain outside Git. No existing output was overwritten.

## Saved results and limits

Working output:
`D:\Nect-codex-output-20261022-task13-f336c94f2b714cd5bc73d6ca646c8f2e\keyer-mask-example-mvj16wu3\model`.
It includes both JSON/SVG/PNG stages, the Invert check PNG, `README-ja.md`,
model/render receipts and a headless screenshot. TEMP/TMP and logs use this task's
new D directory. Scripts close their headless browser and local preview server.

Sealed delivery destination:
`D:\Nect-codex-output-20261022-task13-f336c94f2b714cd5bc73d6ca646c8f2e\keyer-mask-example-artifact-mvj16wu3`.
The external manifest records actual file sizes/hashes and branch/PR identity.

Native GUI input for this sample, physical input, ordinary OS Open/Save/PNG
dialogs and normal OS cold reopen were not tested. These results are headless
Web/model evidence, separate from the earlier native synthetic PNG check and the
unresolved OS-save attempt. PID139120 and its profile received no request, read,
write, close, restart or save from this sample scope. No install/protection
change, main merge, release, new EXE or Library upload.
