# Technology baseline

## Current choice

M0 core: C++20 + CMake + Boost.JSON.

M1 desktop candidate: Qt 6 Widgets around the existing core Session.

This is an implementation decision for the first vertical slice, not a permanent ban on other runtimes.

## Why

The immediate risks are:
- native desktop interaction and docking
- Windows input/IME
- stable C/C++ host/plugin boundaries
- direct control over the document/evaluator
- future Resolve/OpenFX/native codec integration

Qt is kept outside the core library so rendering/UI can evolve without changing authored state.

## Do not infer

- M0 does not prove Qt UI quality.
- M0 does not implement MCP, AI/PSD codecs, OpenFX or full typography.
- Boost.JSON is a bootstrap JSON implementation, not a product-wide serialization doctrine.

Future stack changes require a concrete blocked requirement or measured problem, not preference-driven rewrites.
