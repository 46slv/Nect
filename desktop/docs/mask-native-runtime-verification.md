# PR71/72 native runtime acceptance

Same PR72 product EXE, built from
`deac549164728800d9fd7188943c489f6bf56f69`, 10,011,136 bytes, SHA-256
`fb8e5a01bc0679c5593c8668b4256197576f3f9860155abb1a89a4e1623120ea`.
This follow-up changes verification script/docs only; no product patch or rebuild.

Native Tauri/WebView2 (Edge155.0.4283.62), CDP synthetic input. A fresh runtime
EXE copy and `NECT_SVG_ACCEPTANCE_ROOT` were on D. Exact process start ticks,
EXE and child WebView profile/CDP arguments matched. The application's existing
guard created a new direct-child profile; its Recovery root was inside this new
runtime. No existing document, profile or Recovery was opened or modified.

Five native states passed:

1. Closed-Path Feather sigma4 with rotation/nonuniform scale before Mask.
2. Image alpha with transparent, half-transparent and opaque regions.
3. Keyer alpha plus Feather sigma4.
4. The same Keyer+Feather alpha inverted.
5. Image Position/Size in document coordinates followed by appearance rotation30,
   nonuniform scale2×1 and position[50,30].

For each state the actual Canvas layer markup and the exported SVG were
rasterized in the native WebView; full RGBA matched. Three states additionally
matched the prior sealed headless RGBA hashes exactly. Alpha samples were
0/128/128/255, partial source alpha128×material128 produced64, and normal plus
inverted feather coverage matched source alpha with zero 8-bit error.

The existing Graph picker switched Image→Keyer; Inspector changed Feather and
Invert. Each committed change was one Undo/Redo; repeat edits were neutral.
The final project was CLI-written to the dedicated profile's documents folder,
read back exactly, and reloaded through the existing native debug loader.
Project/IDs/settings and exported SVG matched. No Open/Save/PNG command was used.
PNGs came from native Canvas serialization, with bytes saved by CLI; the native
Export PNG save handler and OS chooser remain untested.

Exit: after the full synthetic project was saved/read back (12,036 bytes,
SHA-256 `e7644ef9e9125c36e7b68402ea61cebcafa2c3ed1665f0badfab8b4dad2d53fd`),
the existing `finish_close` IPC was invoked once. Page closure was observed.
PID55212 (start2026-10-22T05:58:05.5542967Z), WebView browserPID121624 and its
children were absent; port9491 was no longer listening. No force termination,
duplicate close or profile cleanup occurred. Detached-launcher exit code is
unavailable. The UI file remained dirty/unbound because saving was by CLI;
this does not verify the normal Save/close guard workflow.

Evidence: `D:\Nect-codex-output-20261022-task13-f336c94f2b714cd5bc73d6ca646c8f2e\mask-native-20261022`.
`native-cases/receipt.json` has RGBA hashes and the saved fixture path;
`evidence/identity.json` and `exit.json` establish ownership and shutdown.
A separate `mask-native-artifact-20261022` folder seals this evidence without
copying the runtime profile/Recovery. `scripts/verify-mask-native.mjs` requires
the separately launched and identity-proven dedicated instance; it never opens
native file dialogs or overrides the IPC bridge.

Remaining acceptance: physical/OS input, normal OS Open/Save/PNG dialogs, native
Export PNG save handler, normal OS cold reopen, and the normal Save/close guard
workflow. No protected-main merge, release, installer or Library upload.
