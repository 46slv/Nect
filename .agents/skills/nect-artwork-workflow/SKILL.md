---
name: nect-artwork-workflow
description: Use when an agent is asked to make or revise editable vector artwork in Nect, reuse a Nect example, or verify a native project and its SVG delivery. Not a live-desktop MCP connector.
---

# Nect artwork workflow

Keep the editable Nect project and the rendered exchange file as separate deliverables. Use the current checkout and cite the version you actually verified.

1. Identify the product surface: the current Windows Tauri editor lives in `desktop/`; legacy Qt API or MCP instructions are not evidence about it. Check `desktop/docs/requirements-crosswalk.md` for the applicable surface.
2. Start from a known editable example or the user's project. Work from a copy and keep the source untouched. For a Blend smoke test, use `desktop/examples/live-blend.json` with no external assets.
3. Follow [authoring and verification](references/authoring-and-verification.md) only when you need a command, reference project, or validation checklist. Do not paste whole project graphs into an agent prompt.
4. When a shared editing command is present and verified for this surface, use it instead of building a parallel editor. If there is no supported external live route, say so and use the verified UI or on-disk renderer. Do not treat the renderer as a live editor or put a network server in front of it.
5. Report which artifacts were created, which surface was exercised, and any unchecked handoff step. A CLI export is not proof of native Windows UI behavior.
