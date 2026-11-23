---
name: nect-live-editor
description: Use for inspecting or editing the current Nect Tauri desktop document through its approved local API/MCP, including layers, nodes, Blend, undo, saves and live exports.
---

# Nect live editor

1. Identify the actual app and intended document. Discover live tools and permissions before acting. Legacy Qt MCP, the file CLI, the Windows test fixture, and ordinary Tauri are separate routes.
2. Inspect first. Use the current document, tab and revision identifiers where required. Discover stable layer and node IDs instead of guessing from visual order.
3. Use only advertised, authorized operations. Reinspect after edits or target changes. On stale state or a locked layer, do not force the edit or silently unlock it. After a lost reply, check the receipt or actual state before retrying.
4. Preserve the editable project. Use a fresh output for save-copy or export and verify the result. Report fixture, normal app and manual GUI evidence separately.

Read only the relevant section of [the runbook](references/runbook.md). For file-only work use nect-file-export.
