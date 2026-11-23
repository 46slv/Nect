# File export commands

Run from the repository root with the desktop dependencies already installed. Use an existing output directory and unused destinations; substitute real local paths.

## Editable JSON to SVG

```sh
node desktop/cli/export.js desktop/examples/blend-distance.json "/output/blend-distance.svg"
```

Verify nonempty SVG, the root element and its dimensions. The example is tracked and should not change. See [Desktop README](../../../../desktop/README.md).

## Nect SVG to PNG

```sh
node desktop/cli/svg-to-png.js "/output/blend-distance.svg" "/output/blend-distance.png" --chromium-path "/path/to/installed/chrome"
```

Use existing Chrome and at most one of `--scale` and `--ppi`. This accepts fixed, self-contained Nect SVG, not arbitrary SVG. Check the PNG signature and dimensions. See [CLI requirements and sizing](../../../../desktop/docs/svg-to-png-cli.md). The editable source is still the JSON, not the exported image.
