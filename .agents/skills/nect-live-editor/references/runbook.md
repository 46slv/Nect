# Live editor runbook

## Select the route

- Ordinary Windows Tauri: follow [the G19 transport and release gates](../../../../desktop/docs/g19-live-api-transport.md). Use the approved matching signed editor/helper, independently verified certificate fingerprint, and exact-client local pairing on a disposable document for the first pilot. Inspect does not grant edits. If this is unavailable, report the missing condition; do not use a test flag or change trust settings.
- Development fixture: only for disposable projects. Never register the fixture tools as ordinary Hermes tools. Follow [the Windows fixture setup](../../../../desktop/docs/windows-live-editor-acceptance.md), including the exact fixture EXE, reviewed checksum and new receipt path, then run its documented verifier. Record the exact fixture build and commit.
- Legacy Qt: use its own API/MCP only if that app is explicitly the target. Its tools do not imply Tauri support.
- File-only export: use [nect-file-export](../../nect-file-export/SKILL.md); it never edits the open editor.

## Discover and verify

Initialize the MCP session and use `tools/list`. Read the advertised Inspect schema and call it. Distinguish wrong edition, absent tools and missing grants. Match the document/tab and discover the relevant stable object IDs. For an uncertain write preserve its request ID where supported and inspect the receipt/state, not a new write.

Record app/build, tool and grant, before/after identity, produced file and verification limits. For an edit check exactly the intended fields and Undo. For persistence reopen and check authored state, not just pixels. A fixture save is not evidence of the Windows file chooser or ordinary Hermes pairing.
