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
