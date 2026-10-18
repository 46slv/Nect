# Local recovery autosave

Desktop requests recovery after every committed Session operation, including
Undo/Redo, paste/delete, Inspector changes and Text commits. The adapter observes
the operation's final document lifetime in a microtask; there is no debounce
timer. Canvas/IME drafts and gesture previews are not committed snapshots.
Successful imports and restored unsaved documents also receive a copy. Initial
launch artwork is only a baseline.

Writes are asynchronous and globally serialized. While one immutable snapshot is
in flight, each document keeps only its latest waiting state. Fast or synchronous
bursts can coalesce intermediate revisions; the latest pending state drains
automatically after the active write. Every operation requests protection, but
an operation is durable only after its write succeeds. A crash before that can
lose queued/in-flight edits. There is no promise to retain every intermediate
operation or an unbounded history of recovery files. JSON encoding/validation
still costs time proportional to document size; disk I/O is not awaited by edits.

The header keeps manual Saved/Unsaved separate from Recovery pending/saved/failed
and shows requested/saving and last saved revisions. Revision numbers identify
the current editing Session; restore starts a new Session. Existing recovery
records retain their full contents, time and document/window identity, without
an authored-schema change or a requirement for a persisted revision counter.
Click a failed recovery status to retry. Recovery copies opens the restore picker;
startup offers inactive, unsaved copies. Later preserves them. Restore creates an
unbound, unsaved Session: Save As chooses the user's project destination. Image
preflight, font preparation and the existing codec validate before replacement.

Rust owns the app-local `com.nect.svg/recovery-v1` directory. The renderer supplies
contents and a native-issued document token, never a filesystem destination.
Tokens belong to one document and window in one run. A held Windows exclusive
lease prevents another running window/process from being offered for recovery.
Acceptance EXEs use a stable `nect-svg-acceptance-recovery` sibling beside their
disposable profiles, so actual process restarts stay isolated from user data.

Each document retains at most two validated generations (current and previous).
Staged writes flush their bytes before atomic replacement. A failed current
replacement preserves the last good copy; an unreadable current record falls
back to its previous generation. A 50 MB project bound includes embedded fonts
and images; snapshots retain the exact serialized contents. No manual project
binding is used. Recovery does not mark the manual project saved.

Restored source copies are acknowledged only after a durable successor copy.
The source files remain present. Clean manual saves mark that recovery generation
non-recoverable. Discarded unsaved documents may remain available as recovery
copies. There is no automatic purge of old document/run directories or existing
legacy recovery data. The two-generation bound is per document, not a global
disk quota. Broken/unrecognized files remain untouched. Hardware/filesystem
failure beyond atomic replacement is outside the guarantee.

The separate C++/Qt recovery system uses a different document contract and stays
unchanged. No schema, graph, Undo, font shaping or source-file authority changes.

Verification: `tests/recovery-autosave.test.js`,
`tests/session-commit-notification.test.js` and existing lifecycle/Text/gesture
checks; `tests/recovery-per-operation.e2e.js` explicitly simulates slow/failing
disk in production CSP UI. Native `recovery_storage::tests` and the existing
atomic/window/two-generation storage stay unchanged. Actual EXE latest-state
restart, failure and retry receipts accompany the trial build.

Review gate invoked for failure recovery/storage design. Existing ownership and
the local isolation/failure tests settle the bounded contract; external review
not-required, external submissions 0.
