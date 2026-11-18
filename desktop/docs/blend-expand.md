# Blend: explicit expansion

REQ-138 / #302 requires intentional conversion to independent editable geometry,
not a geometry-only live Convert to Path operation that drops appearance.

`expand-blend` is a CommandSession command. It evaluates the selected active
Blend, preserves source A/B graphs, replaces that Blend's identity with an
ordered painted Join, and adds editable Multi Path plus solid Fill/Stroke nodes
for each evaluated copy. Ordinary edits remain live; only explicit expansion
breaks the Blend dependency. One Undo restores the exact authored Blend graph.
The existing schema and nodes are reused; no baked proprietary geometry format.

Expansion rejects locked/bypassed nodes, parameter references which would lose
their meaning, blocked export appearance, and unsupported or excessive output.
It validates a private document candidate before committing. Initial acceptance
must cover appearance, source independence, point editing, Undo/Redo and reopen.

## Creating a live Blend from selection

`blend-selected` takes exactly two stable node IDs in selection order (A then B),
validates their live Blend output, and adds one connected Blend in one History
entry. The graph toolbar exposes this action for two selected nodes. Existing
source nodes and Output wiring are preserved, just like ordinary Add node; the
new Blend is selected for preview, and the user can connect it to Output. Invalid
paint/topology or missing sources reject the entire command without a partial node.

## Bounded acceptance measurements

On 2026-11-17, Node 24.14.1 on the development Windows machine, the bounded
`node scripts/benchmark-blend.mjs` probe measured five calls after one warm-up:
4-point input / 100 Steps: median 0.697 ms, max 1.273 ms; 128-point input /
100 Steps (13,056 output points): median 10.829 ms, max 16.846 ms. This is the
synthetic geometry/paint evaluator only, not Session, rendering, IPC, native GUI
responsiveness or a universal timing guarantee. The benchmark is deliberately
not a timing-sensitive CI assertion and does not run automatically.

CommandSession regressions separately cover Steps 0/1/5/100, deterministic
re-evaluation, source edits and reopen, plus source deletion diagnostic,
Undo recovery and explicit disconnect/reconnect without stale geometry.

## Editable example

Open [live-blend.json](../examples/live-blend.json) through Project → Open JSON.
The [short guide](../examples/live-blend.md) explains editing the connected sources and Steps;
[live-blend.svg](../examples/live-blend.svg) is its initial vector appearance.
This reusable example does not replace pending native-window acceptance.

## Specified Distance: bounded follow-on design

REQ-138 (Confirmed, Should) and issue #302 permit staged spacing support.
[Adobe's definition](https://helpx.adobe.com/illustrator/desktop/manage-colors/apply-transparency-and-blending/blend-options-overview.html)
measures corresponding edges, not the empty gap between shapes. Its public page
does not specify rounding, numeric limits or a general deformation metric.

This Nect stage supports two identical closed contours related by translation,
after the existing seam/winding correspondence. Relative Bezier handles must
match; scaling and morphing remain available in Steps, not Distance. Colors may
still interpolate. Distance uses evaluated document coordinates (document px),
not screen pixels or stroke bounds. It is a **maximum interval**: for translation
length L and positive finite d, use max(1, ceil(L/d)) equal intervals, retaining
both endpoints. At most 255 intervals and the existing 100000-point budget are
allowed; overflow diagnoses rather than truncates. Coincident inputs retain the
two endpoints. These rounding and scope choices are Nect's, not claimed Adobe
numeric parity. Spine, Smooth, rotation and multiple sources are not this stage.

The typed Blend declaration adds distance and Steps/Distance operations. Legacy
generic Boolean operations migrate to Steps; Steps values and old documents retain their meaning.
Distance does not overwrite Steps. Author edits use existing CommandSession
parameter/operation commands and History; computed copy count is never authored.
Codec roundtrip, Undo/Redo, source edits, SVG and explicit expansion must preserve
this distinction. The inspector must state the translation-only limitation.
Invalid or unsupported inputs produce diagnostics, not stale output or partial
commands. Native GUI acceptance remains separate from headless regression.

The evaluated coordinate space is the common Composition/document space, before
viewport zoom. For n intervals the actual spacing is L/n and there are n-1
intermediate shapes. This is not a stroke-outline gap. All anchors and relative
Bezier handles must match after seam/winding alignment; closed status and segment
count must match. Every corresponding control therefore undergoes the same
translation. Comparison uses an absolute 1e-9 document-unit tolerance, not a
coordinate-magnitude-relative tolerance that could hide a large deformation at
large coordinates. Differences lost to floating representation cannot be recovered;
representable large/small cases are tested, and uncertain mismatches diagnose.
Source edits that violate this condition produce an error, never a silent switch
to Steps. The user can explicitly select Steps to continue morphing.

Legacy generic Boolean operation tokens were ignored by Blend. Reading them
normalizes to Steps and supplies the new distance default; this is semantic and
rendering compatibility, not a promise of byte-identical reserialization.
