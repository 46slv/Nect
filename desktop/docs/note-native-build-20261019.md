# SS-Note-ROG native candidate build, 2026-10-19

Application source: `7991d86f66db0dd3587eddd34acb604d514eb73e`, fetched from
`https://github.com/46slv/Nect.git`, branch
`codex/req23-named-presets-20261019` (Draft PR32). This recovery branch adds only
scripts and documentation; the application inputs remain identical to that
candidate. The unavailable other-PC commit `f54505a` is not included.

## Prerequisites and recovery

The owner authorized necessary official prerequisite installation and private
backup on 2026-10-19. Existing applications and older worktrees were preserved.

- Installed from the [official Rust installer](https://rust-lang.org/tools/install/):
  minimal `stable-x86_64-pc-windows-msvc`, rustc 1.99.1
  (`58cf8f9c3 2026-10-13`), Cargo 1.99.1 (`5f94df478 2026-08-27`),
  rustup 1.29.1 (`d95a37b6a 2026-08-13`). The normal installation prompt was
  inspected before proceeding; no explicit legal-terms prompt was presented.
  The installer added `%USERPROFILE%\.cargo\bin` to user PATH.
- Already installed: VS Build Tools 2022 17.14.37314.3, MSVC 14.44.35207,
  Windows SDK 10.0.26100.0, WebView2 155.0.4283.62, Node 24.15.0,
  npm 11.12.1. No Microsoft component, Node, or WebView2 installation was needed.
- Locked npm and Cargo dependencies were downloaded only for the isolated checkout.
  Official Tauri CLI 2.12.3 was pinned for the build invocation; Tauri Rust remains
  the candidate's locked 2.9.0 dependency. No dependency lockfile was changed.

Use an independent clone; do not reset an existing user worktree. Fetch this
recovery branch, check origin and the commit, then run in Windows PowerShell 7:

```powershell
cd desktop
.\scripts\build-native-candidate.ps1
.\scripts\test-native-launch.ps1
```

The build script checks the repository origin and compares tracked application
inputs with the declared source commit; untracked application inputs also refuse.
It runs `npm ci`, the bounded 90-test Node set, production frontend build,
`cargo test --locked`, installed Japanese TTC smoke, and the standalone Tauri
release build with `--no-bundle -- --locked`. It produces
`src-tauri/target/release/nect-svg-desktop.exe` and `test-results/native-build.json`.
No installer or release is created. Inspect each stage's log, including explicit
font `NOT RUN` messages; process exit alone must not upgrade skipped checks.

The launch script creates a unique acceptance root directly beside that EXE.
Nect confines the test WebView2 profile/documents to the root and separates
recovery data from ordinary user app data. The script observes only the process
it launches and then closes/stops it. It never opens user projects or terminates
another Nect instance. Keep the generated acceptance roots local; they are not
backup deliverables. `test-results/native-launch.json` records the result.

## Verification boundary

- PASS: source identity and unchanged application inputs; npm locked install;
  production frontend bundle (existing large-chunk warning); 90 focused Node
  tests; 39 native Rust tests; installed Meiryo TTC extraction, embedding,
  horizontal/vertical outline and JSON reload smoke; optimized standalone EXE.
- PASS, limited process smoke: 15-second survival; responding native window named
  `Nect SVG`; nonzero window handle; isolated WebView2 profile creation.
- UNVERIFIED: graceful application close. `CloseMainWindow` returned true but
  the process did not exit in five seconds; only the disposable process was
  terminated. A following process query found no matching app or acceptance-root
  WebView2 processes. Without native UI observation, the cause is unknown.
- NOT RUN: actual native project save/close/cold reopen/re-edit, context menus,
  numeric-expression interactions, named-preset interactions and physical IME.
  The available tools expose no native `node_repl`/Computer Use runtime; the CUA
  surface explicitly disables native APIs. No browser test is presented as native
  verification. A human or a native UI-enabled session must exercise these rows.

The EXE is an unsigned development candidate. No security setting was changed,
no safety warning was bypassed, and no main merge, release or unrelated feature
work was performed. Distribution backups contain only the built EXE and a bounded
validation manifest, never Cargo/npm caches, installed font bytes or user files.

The validation manifest is authoritative for the final EXE size/hash and private
Library identity. Git push/remote-SHA and Library backup completion must be checked
independently; local file existence is not a cloud-backup claim.

External review gate: invoked; `not-required`, zero submissions. This build and
the bounded recovery scripts have direct local evidence and no unresolved design
decision. The limited close observation is preserved as an unknown, not repaired
speculatively. Debugging owner: `~/.agents/knowledge/shared/coding/debugging.md`.
