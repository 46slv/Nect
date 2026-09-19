# Nect

Nect is an experimental 2D graphics authoring engine/editor focused on:

- fully addressable Bézier points and handles
- non-destructive property-driven editing
- expressions and cross-object references
- vector + raster + compositing in one document model
- Illustrator/Photoshop interoperability
- first-class API/MCP automation
- local-AI-friendly structured document semantics

This repository is the technical source of truth for implementation.
Product requirements, decisions, and research evidence are maintained in the linked Notion specification hub.

## Current status

**M0 bootstrap / kernel baseline**

The first engineering target is deliberately narrow:

1. create a document and Bézier path
2. edit point/handle properties numerically
3. bind one property to another by stable ID
4. save and reopen the native document
5. mutate the same property through the external command surface
6. undo/redo
7. export SVG

The full PS/AI parity backlog is **not** implementation authorization.

## Project rules

- One canonical authored document state.
- GUI, API, and MCP must share the same core commands.
- Stable IDs are not array indices.
- Unsupported interoperability must be explicit; never silently flatten or discard.
- Prefer the smallest correct implementation boundary over speculative managers/services/frameworks.
- Candidate requirements remain backlog until explicitly selected by the current milestone.

## Bootstrap

The M0 implementation baseline is being migrated into this repository under `bootstrap/m0`.
