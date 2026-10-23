# R04-PUTINSIDE-APPEARANCE-01 — Confirmed organization safety

Status: Sol-frozen implementation packet, 2026-09-27. Baseline `codex/practical-alpha@19872409c02bad8b89b013f8486c596c6619e84d`, native 0.20.

Authority: DEC-71 Mission ownership; [REQ-52](https://app.notion.com/p/3e0fd279a6f38151b544dda1a39adf25) and [P04-A](https://app.notion.com/p/3e3fd279a6f381ee8ab0ea5ea614c006). `ARCHITECTURE.md` requires separate coordinate and appearance preservation. This packet chooses REQ-52's explicit refusal branch for an unsafe destination; it does not add an automatic conversion or flattening path.

## Observed gap

`PutInside` currently accepts a contiguous sibling block immediately before a Group and verifies world transform preservation. It does not check whether the destination Group is hidden, translucent, masked, blended or isolated. The same command can therefore commit a structural move that changes the rendered result without declaring an appearance conversion.

## Contract and oracle

Keep `PutInside`'s input shape, native format, history and neutral destination behavior. Before changing the candidate, require the destination Group to be visible, normal blend, not isolated or masked, opacity literal 1 without a driver, and without effects. Reject an unsafe destination with `PUT_INSIDE_APPEARANCE` and a reason naming the unsupported appearance boundary. Session must leave Document, revision, history, collection membership, references and evaluated render unchanged. The JSON-lines API and Desktop action receive the same shared failure. A source sibling's own compositing is not, by itself, a reason to refuse a neutral pass-through container.

Positive oracle: the existing transformed neutral destination with immediately preceding A/B keeps world transforms, authored refs and drawable order, and retains one Undo/Redo and native roundtrip. Negative oracle: each unsafe destination state (hidden, opacity below 1, opacity driven even when currently 1, non-normal blend, isolation, mask) rejects atomically. An invalid/noncontiguous selection continues to use its existing scoped error. No new general cross-parent movement, Collection or definition/instance semantics, and no Candidate REQ-54/56–59 implementation.

## Acceptance and residual

Focused compositing/API tests and affected Desktop tests pass. This closes only the unsafe-destination refusal part of Confirmed REQ-52. It does not prove appearance preservation for arbitrary reparenting, nor complete REQ-30 movement in/out, tree hands-on acceptance or R04 shared parts.

## Local result — 2026-09-27

The reviewed candidate checks destination visibility and compositing after the existing contiguous-sibling validation and before evaluation/mutation. The shared `PUT_INSIDE_APPEARANCE` failure names the specific boundary; neutral destinations retain the existing command and native format. Focused tests cover hidden, translucent, expression- and binding-driven opacity, blend, isolation and mask refusals; noncontiguous error precedence; a composited source sibling moving into a neutral destination; and JSON-lines error/unchanged Session state. A Group effect stack cannot currently be authored in a valid Document, so that future-facing guard has no separate valid fixture. Release `compositing_tests` build and `compositing_contract` passed 1/1; affected `window_interaction` also passed 1/1. `git diff --check` passed. No hands-on GUI claim follows from these core/API/offscreen Qt tests.
