# Local recovery autosave

Desktop observes committed Session encodings. Changes settle for 1 second before
writing; continuous commits request a copy within 10 seconds. Canvas/IME drafts
are not committed snapshots. Successful imports and restored unsaved documents
also receive a copy. Initial launch artwork is only a baseline.

The header keeps manual Saved/Unsaved separate from Recovery pending/saved/failed.
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

Verification: `tests/recovery-autosave.test.js` and existing lifecycle checks;
native `recovery_storage::tests`; isolated real-EXE restart/restore, full snapshot
equality, write failure and retry receipts accompany the trial build.

Review gate invoked for failure recovery/storage design. Existing ownership and
the local isolation/failure tests settle the bounded contract; external review
not-required, external submissions 0.
