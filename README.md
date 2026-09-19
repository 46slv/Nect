# Nect

> **Fresh implementation agent:** start with [START_HERE.md](START_HERE.md).  
> A repo link plus that file is intended to be enough to resume current work without old chat history.

Nect is an experimental 2D graphics authoring engine/editor focused on:

- fully addressable Bézier points and handles
- non-destructive property-driven editing
- expressions and cross-object references
- vector + raster + compositing in one document model
- Illustrator/Photoshop interoperability
- first-class API/MCP automation
- local-AI-friendly structured document semantics

This repository is the technical source of truth for implementation.
Product requirements, decisions, and research evidence live in the Notion specification hub:
https://app.notion.com/p/3dffd279a6f381cca8c7c4dec111b131

## Current status

**M0 kernel baseline**

Implemented in M0:

1. create/load a small native document
2. address every Bézier point and handle property by stable ID
3. bind compatible properties across objects
4. reject cycles, invalid references, invalid units, unknown fields and partial batches
5. save/reload authored state
6. undo/redo through one Session owner
7. export a declared SVG subset
8. expose a local JSON-lines command adapter for black-box testing

Not implemented yet:

- Qt desktop UI
- formal MCP server
- AI/PSD codecs
- OpenFX hosting
- full typography
- raster/compositing production model

The PS/AI parity backlog is **not** implementation authorization.

## Build

Requirements:

- C++20 compiler
- CMake 3.24+
- Boost.JSON headers
- Python 3 for black-box tests

Example:

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
python3 scripts/smoke.py --exe build/nect
```

## Entry points

- `START_HERE.md` — single entrypoint for a fresh Astra/Sol/Codex session
- `CURRENT_GOAL.md` — current implementation scope
- `AGENTS.md` — protected implementation boundaries
- `ARCHITECTURE.md` — ownership and data-flow boundaries
- `docs/model-v0.md` — native model semantics
- `docs/quality.md` — anti-slop engineering contract
- `docs/first-usable.md` — M1 acceptance flow
- `schemas/native-v0.1.schema.json` — native JSON shape

## Project rules

- One canonical authored document state.
- GUI, API, and MCP must share the same core commands.
- Stable IDs are not array indices.
- Unsupported interoperability must be explicit; never silently flatten or discard.
- Prefer the smallest correct implementation boundary over speculative managers/services/frameworks.
- Candidate requirements remain backlog until explicitly selected by the current milestone.
- No distribution license has been selected yet.
