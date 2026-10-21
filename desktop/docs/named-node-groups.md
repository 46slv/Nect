# Named groups for placed graph nodes

Base: verified PR45 `6c536f0a98e6a7e82120e860b0fbb5599cc8f4c5`,
`codex/scoped-point-selection`, remote `https://github.com/46slv/Nect.git`.
The current parent brief explicitly selects this new capability. Existing
`node-library-categories.md` excludes placed-node groups from its older category
slice; that exclusion is not authorization. Current core/main have no equivalent
placed-node group storage or commands. Legacy C++ structural Group and Desktop
painted geometry groups are separate concepts and stay unchanged.

V1 stores optional composition-local `nodeGroups` records with stable ID, name
and member node IDs. Groups are flat; a node belongs to at most one. Empty groups
are retained explicitly, including after member deletion, and can be renamed,
refilled or ungrouped. Undo of node deletion restores the original membership.
Create requires a nonempty, distinct, current, ungrouped node selection. Add
rejects nodes belonging to another group; Move into group explicitly transfers the
entire selected set into the chosen group in one atomic Undo. Remove affects only
selected members of that group. Ungroup removes the record, never the nodes.
Missing/stale IDs, malformed names, duplicate membership and locked compositions
reject without mutation. Membership is organization metadata, so parameter-driven
or individually locked nodes can belong to a group without changing their values.

Use common Session commands and validation/history, with a new Desktop schema
only when nodeGroups is authored. Old documents without groups remain untouched.
Node IDs, positions, connections, parameter values, evaluation and SVG output are
unchanged by group commands. Duplicated/pasted/preset nodes start ungrouped;
composition duplication preserves groups with fresh group/member IDs.

UI contract: the existing Graph is the working surface. A compact collapsible
Groups strip contains group/name selection, Create from selection, Rename,
Add selected, Move into group, Remove from group and Ungroup. Names/counts and a
small per-node group label show membership. Inline names remain drafts until an
explicit commit; Cancel/Escape is neutral. Capture layer/session/revision and
selection IDs for actions so stale controls cannot affect another document.
Use existing small controls, thin neutral separators and escaped labels. No
Add-node category picker, frame bounds, per-frame measurements, group movement,
collapse/visibility/solo/nesting or overall panel layout changes.

Focused checks: all commands/atomic rejection and one-Undo/Redo, persistence,
empty groups, deletion/duplication identity, document switch and stale controls;
unchanged evaluation/SVG and existing node IDs/positions/connections. Actual
native WebView2 interaction should exercise the real controls and saved JSON
reload. Physical input/IME, OS Save/Open and cold-process reopen remain separate
unless available. Preserve PR40 through PR45 sealed candidates before rebuilding.

## Verification receipt

Final runtime source: `946c2e89235049d0c426f25de16ab740b1df4647`.
Group schema 44 is adopted only on first creation. Documents without nodeGroups
keep their existing version/serialization; removing the last group follows the
existing monotonic version policy. Undo restores the prior complete document.

- 71/71 focused model/DOM tests passed across groups, existing node
  duplication/clipboard, single/batch artwork duplication, Add-node categories,
  node context menu and graph snapping. A final 20/20 recheck covers group
  model/DOM and existing selected-point delta after label corrections. Counts
  overlap. Final diff leaves preexisting point, text and node-stage labels intact.
- Group checks cover one-Undo/Redo for every command, fixed node/position/wire
  snapshots and unchanged evaluated geometry/SVG, JSON roundtrip, no-op/Redo
  neutrality, empty retention/deletion, protected Output deletion, locked
  composition, locked/driven values, stale actions/documents/selections, invalid
  names/IDs/membership/nesting/version, escaped labels and Cancel/Escape/IME guards.
  Node copies/presets stay ungrouped; composition/artwork copies receive fresh
  group IDs and remapped member IDs.
- Final actual Tauri/WebView2 native candidate passes 3/3 synthetic-CDP cases:
  create/rename/explicit membership move with one-Undo/Redo; neutral invalid/name
  drafts plus deletion Undo/Redo and empty retention; disk JSON read/write and
  renderer reload, ungroup Undo, and document switch. Node IDs, positions,
  connections and exported SVG are unchanged by group operations. Zero page
  errors. Both final screenshots inspected. The runner waits for the owned WebView
  target to navigate and reselects a recovered node after deletion Undo, following
  existing Graph selection behavior.
- Frontend production build (141 modules), offline native release/custom-protocol
  build, native config, scoped formatting and diff checks pass. Existing bundle
  warning remains. No group boundary measurement or animation-frame task is
  introduced; membership lookup is built once per structural render. No broad
  frame-time benchmark is claimed.

EXE: 9,999,872 bytes, SHA256
`46c4ab91ad2a113e700edd17ea5e13c8e5026fedb986aab8f037ce871121d2ca`.
Embedded bundle `index-CJArJoQl.js`, SHA256
`b60a041953e12e609e1fd53385758dd97ba08c840caa3978ec6694c33ec42713`.
Native PID 130264 was closed after verifying its task-owned executable path.
Evidence lives in `test-results/named-node-groups`, with final `native/receipt.json`
and `native-final.log` distinguished from earlier diagnostics/intermediate runs.
The task output is sealed separately as `named-node-groups-artifact-946c2e8`.
PR40 through PR45 sealed candidates are retained unchanged by hash.

Not run: physical keyboard/IME, native OS Save/Open dialogs, cold-process file
reopen, OS scaling, unrelated full audits and broad graph-performance benchmarks.
The JSON reload check is narrower than those OS file flows. Disabled native
computer APIs were not bypassed. No in-scope implementation blocker remains.
