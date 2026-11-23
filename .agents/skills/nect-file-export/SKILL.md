---
name: nect-file-export
description: Export existing editable Nect Tauri JSON to SVG or rasterize fixed Nect-exported SVG to PNG without using the open editor. Use for previews, asset batches and reproducible exports.
---

# Nect file export

- Resolve the exact local input and a fresh output path. Keep the editable JSON untouched. Never replace an existing output silently.
- Read [the commands](references/commands.md) for the requested format. Use the checkout CLI, not legacy Qt MCP or a live editor test fixture. Use nect-live-editor if the user needs the current open document.
- Check the process result and resulting file. For PNG, verify pixel dimensions. Report the actual product and any unsupported inputs instead of silently substituting another converter.
