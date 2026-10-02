# Nect platform and real-GUI qualification — 2026-10-02

This receipt preserves distinct source checkpoints and evidence surfaces. It does
not claim an all-green aggregate, whole-Requirement completion, or release approval.

## Windows automated checkpoints

- `bba27ee8c5b2e7def64310f9b8efdd7aeb48e04b`, tree
  `c47824d142b28cefe64a4749506d0251a128abd0`: Release/deploy PASS;
  full72 CTest71pass/0skip/1fail. See `windows-verification-receipt.md`.
- Utility candidate `cb864804aaccfe73b205405db5185174c44e81e6`, tree
  `53bfc5c96c524f209f7a19b2cacbc7637e355cee`: Release/deploy PASS;
  full74 CTest73pass/0skip/1fail. Dedicated Utility278/279 checks at DPR1/2
  include capture-file save; existing `window_interaction` PASS. Remote branch
  `dot/windows-utility-icon-20261002` points directly to the unchanged candidate.
- The remaining `folder_library_contract` failure is an owned registry scratch-key
  creation denial. Separate diagnostic confirms access denial; no security/ACL or
  assertion change was made. It remains FAIL, not SKIP/PASS. No product repair was
  isolated from that environment failure.
- Actual Utility checkout was the fresh task-3 workspace; predecessor task-2 was
  read-only. An earlier inference from unchanged task-2 was incorrect and explicitly
  corrected. The actual task-3 branch, clean state, result and full-log hash were
  independently read back after completion. Requested cwd is not proof of the actual
  writable consumer workspace.
- Utility full-log SHA256:
  `b255d07c57879e7582ec9065299415aa70ca3693020be1213155cde1cabb6d69`.
  Captured glyph/state cells were visible; caption fonts rendered as boxes. Native
  Windows GUI acceptance was NOT_RUN because the worker lacked that CUA capability.
- Requested worker model was Luna Max; the effective runtime label was not exposed
  for independent verification. Source/build ownership was released at terminal.

## Actual own-cloud GUI checkpoint

Source `740cd0b48f258fd95fe4419bd61e6b0127bf1876`, tree
`7df324245c55aeb6fd200106cceab9320b36107b`. The copied immutable desktop binary had
SHA256 `872b0ad3ec830b5ffb6cf1df41946419cda4cf9074b6bbf2e4f36cd80d24bd82`.
The Linux/Xfce desktop was1364×1024, DISPLAY=:0. CUA observed real application
windows and performed native input; this was not an offscreen test replay.

Normal1180×812 and125%1250×813 windows passed these bounded observations:
- Persistent distinct Guide/Grid/Snap icon ON/OFF, independent state and stable
  observed bounds; actual hover/tooltips, accessible names, keyboard focus/Space
  and Snap menu parity.
- Fit, Setup and Keys actually opened. At1000 logical width, Utility horizontal
  scrolling preserved reachability. View-only interactions retained revision0.
- Polygon and Star numeric725 displayed authored725 and modulo5. Actual clockwise
  drags changed geometry/numeric value and exactly one revision; one toolbar Undo
  restored725 for each. Source/object identities survived native persistence.

Normal view-only Save As was independently compared with the baseline: exact
SHA256 `524562da938c4698962deacb79a09bb6f44a8d081e0446d1545ca58c8c3fe0a8`.
Normal authoring output and125% authoring output both have SHA256
`4e6ed8c0f1505b72c626a8986a1664a7351d8edc680430ec856244bebf317ed5`, containing
both725 literals with original `gui-polygon-source` / `gui-star-source` IDs.
High-DPI view-only native Save As was not separately repeated; its revision/state
checks and normal exact-byte proof remain distinct.

An extra150% case required1500×975 physical at the existing1000×650 logical
minimum, wider than this cloud display. It is partial environment evidence, not a
source defect conclusion;125% supplied the fitting high-DPI qualification case.

GUI NOT_RUN: Escape during held drag (supported drag action is atomic), disabled
state, binding/expression, stale-context and no-net negatives. Dedicated automated
checks cover separate cases; do not claim they were manually repeated. Windows-
specific, third-party-host and subjective acceptance are not established by Linux
GUI observations. Screenshots were captured/reviewed in CUA, but no durable exported
screenshot artifact is claimed. All owned Nect windows were closed and runtime released.

Ignored detailed receipt `build/d0-evidence/gui/objective-gui-receipt-740cd0b.json`
SHA256 `4f1f5052259d9049ab3ffb995c606feafd46fd81b2882f3132cee3a1bd50d6f7`.
All three run app logs were empty. Baseline fixtures and final saved-file hashes
were independently checked by the integrator.

## Formal MCP classification

Source review at740cd0b confirms the six passing Windows MCP contracts are genuine
MCP2025-06-18 JSON-RPC stdio integrations. They initialize a separate Python sidecar,
exercise tools/resources and forward into a separate desktop-owned Host/Session
through a Windows named pipe. The backend JSON-lines IPC alone is not MCP.
Existing tests include identity, source/save and several cold-process reopen cases.

A separate named commercial MCP client and exhaustive protocol conformance were
not tested, but no new generic gate of that kind is required by current REQ-60/61/62,
which are already VERIFIED. Do not erase proven formal integration by describing
these as merely MCP-named tests. Resolve/OFX is a separate external-host domain.

Explicit D0 wire gaps identified at this checkpoint: Template Artboard duplicate,
scoped Guide alignment with `guide_artboard`, and inherited `grid:` alignment/
distribution. A separate test/docs-only parity packet addresses those cases; its
new formal Windows execution must be recorded separately. Guide committed update/
override commands already cross MCP; pointer gestures are not a new MCP operation.
