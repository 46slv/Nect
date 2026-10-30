# Fixed Nect SVG → PNG developer CLI

Use an existing Node/development dependency checkout and an **installed** Chrome.
The adapter does not install/download a browser, listen on a port, or open the editor.
From the repository root:

```sh
node desktop/cli/svg-to-png.js artwork.svg artwork.png --chromium-path /path/to/chrome
node desktop/cli/svg-to-png.js artwork.svg artwork-2x.png --scale 2
node desktop/cli/svg-to-png.js artwork.svg artwork-300ppi.png --ppi 300
```

For the second/third examples, set `CHROMIUM_PATH` to the installed executable first.
`--chromium-path` overrides it. Paths are local files, never URLs. `--help` prints usage.
This is a developer adapter, not a packaged CLI distribution.

## Accepted input and sizing

- Fixed SVG exported by Nect, at most 50,000,000 UTF-8 bytes. Requires positive finite
  root width/height (unitless/px/mm/cm) and `viewBox="0 0 widthPx heightPx"`.
  Responsive/percentage sizes, missing viewBox, and unsupported markup fail explicitly.
- ViewBox dimensions are CSS pixels at 96 px/in. Root mm/cm lengths are **not**
  converted again. Default scale is 1; `--scale` and `--ppi` are mutually exclusive.
- Reuses production `pngDimensions` and `rasterizeSVG`: scale 0.1–8, nearest-pixel
  rounding, minimum one pixel, maximum 8192 pixels per side and 16 megapixels.
  PPI uses the existing PPI/96 sizing rule (not the scale-preset range) and writes
  rounded pixels/metre to PNG pHYs; e.g. 300 PPI is 11811 pixels/metre.
- Nect paths, gradients/masks and their internal fragment references are supported.
  Canonical embedded PNGs follow existing asset size/checksum/aggregate rules and
  are decoded before rendering. Nect text must already be exported as outlines.
  Active content, foreignObject, text/font dependencies, arbitrary CSS, external
  resources, non-PNG data URLs, unknown elements/attributes and missing fragments
  are rejected. This does not promise compatibility with arbitrary third-party SVG.
- Browser modules are supplied by an exact fixed virtual-origin Playwright route;
  all other network requests are refused. No HTTP server or external fetch API.

## Output safety and limits

The output directory must already exist and the output path must not exist (including
symlinks). Input SVG/JSON is never rewritten. PNG bytes are prepared in memory, then
written to an exclusively created same-directory temporary file. Only after close is
an atomic no-clobber hard link used to publish the complete output. A competing
output wins without being overwritten. Filesystems without hard-link support fail
safely; there is no overwrite/copy fallback. Handled failures clean only this
operation's temporary file and do not publish partial PNG bytes. Forced termination,
power loss, directory replacement by another actor and post-publication cleanup
failure are not crash-durability guarantees; a private temporary file may remain.

The existing Windows Chrome CI executes the actual CLI process and checks PNG
pixels, dimensions, PPI metadata, source hashes and rejection behavior. This is not
native WebView2/OS dialog, physical-print/color-management or packaged-release acceptance.
