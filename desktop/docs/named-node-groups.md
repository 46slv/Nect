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
