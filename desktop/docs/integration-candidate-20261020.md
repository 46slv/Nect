# Combined development test candidate

Status: source-verified candidate, Windows EXE NOT_BUILT / NOT_DISTRIBUTED. No binary checksum or Library attachment exists for this candidate. UI/native/frame-time acceptance and the user's original dither file remain NOT_RUN.

Baseline is acca2616ab38c703bcf3e4b0ef465eeae7c62ffc. [Draft PR38](https://github.com/46slv/Nect/pull/38) uses the new codex/nect-test-candidate-20261020 branch; it does not merge into protected main. Original PRs/branches are unchanged.

| Packet                                                       | Product commit                           | Included branch/documentation snapshot                                                                       |
| ------------------------------------------------------------ | ---------------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| [PR35 Layer ordering](https://github.com/46slv/Nect/pull/35) | cae664a60ca827d90583d2e1bedad178bc9870a6 | 646f80b0b116f0a8bc52f1f9a10dc081e9ea3458                                                                     |
| [PR36 dither mask](https://github.com/46slv/Nect/pull/36)    | efb0160b03996891c97b5738133f7fb5f0732ea2 | fb36fb8d2c395f7017447fe7cb76412559a038b3, including Image/Shape retention and legacy-evidence distinction    |
| [PR37 graph autopan](https://github.com/46slv/Nect/pull/37)  | ec474b142296a67c23519389baa904c2aa156a79 | ada9ac547a8d6d0e0a9e9fb4d667a443c5fe5309, including product-spec, interaction and view-navigation acceptance |

All snapshots are ancestors through ordinary merge commits. The only conflict was the two CURRENT_GOAL.md checkpoints; their evidence/residuals were reconciled into one integration checkpoint. No product implementation was rewritten. A source-union check confirms main.js equals PR35's file with only PR37's graphAutoPan function substituted, Layer-ordering files equal PR35, and luminance-mask.js equals PR36. PR37's three specification files match ada9ac5 exactly.

Product tests/build ran at 28a5b9cdb872309e62e76d5032add974afbdf9d8. The exact source with all parent documentation is 6c6c0a5427dbc3bc59ea206bffdb232a195d41f5; its application inputs are identical. Later integration closeout documents do not change those inputs.

## Verification and residuals

- Combined focused tests: 63/63 PASS. Files: layer-reorder-dom, artwork-ordering, artwork-ordering-dom, dither-input-budget, luminance-mask, graph-autopan-work-dom, graph-node-snap-dom, graph-node-snap, gesture-session, command-session and direction-snapping (all .test.js).
- Production npm run build: PASS, 137 modules; existing large-chunk warning retained. main.js syntax, scoped Prettier and diff checks: PASS.
- Exact Layer records/selection, one Undo/Redo, no-op Redo, cancellation/stale/lock guards, SVG order and JSON reload remain covered. Both dither algorithms handle live Text and 1200-point vector fixtures with finite preparation/work/output limits. Graph idle/clamped60-frame work counts are0; pointer and actual-scroll updates retain their independent semantics. These are source/synthetic-DOM checks, not native input or perceived-speed measurements.
- Full suite: NOT_RUN. The following separate probes retain known baseline failures; their assertions were not changed to manufacture a full pass.

| Existing test                   | Baseline acca2616 and integration result                                       |
| ------------------------------- | ------------------------------------------------------------------------------ |
| image-luminance.test.js:398     | vector fingerprint actual93ed6708... vs expected85550975...                    |
| luminance-instances.test.js:148 | schema24 vs21                                                                  |
| luminance-tone.test.js:139      | playground fingerprints cf37bb12... / e3a899c6... vs older stored expectations |
| graph-overview-dom.test.js:124  | 0 overview nodes vs6; reproduced on acca2616 and integration                   |

Raw focused/build/known-failure logs, baseline-marker comparisons, parent-source union and hash/size receipt are under ignored desktop/test-results/integration-candidate/. Earlier independent baseline dither logs remain under the isolated dither worktree. No unrelated tests or product behavior were repaired.

The Image-output dither requirement remains OPEN. Image input and generic Image Output forwarding do not constitute an Image-producing dither. Both current algorithms return Shape/PaintedPath; document PNG export is not a substitute for an Image-typed result. No Image-output feature is added here.

## Windows build handoff

The existing toolchain evidence reports cargo/rustup/rustc unavailable, Windows SDK26100 absent and MSVC unresolved; WebView2 was present. Do not repeat that unchanged failure on this host, install tools, change authentication, or delete files for capacity. This task did none of those operations.

A qualified Windows x64 build host needs an already installed/configured MSVC x64 build shell, compatible Windows SDK, Rust x86_64-pc-windows-msvc toolchain, Node/npm, prepared locked npm/Cargo dependency caches, and WebView2 for eventual human testing. The previously corrected standalone build used cargo/rustc1.99.1 and the same npm-build then Cargo custom-protocol route; see verification.md, Corrected standalone Windows build / 2026-10-19. Those are prior build facts, not evidence that this integration EXE exists.

On that qualified host, use a fresh D: checkout of the integration branch. The commands below run from desktop, pin the verified product inputs, stop on a failed stage and preserve prior artifacts. On another host set PreparedNpmCache to that host's already populated cache. Missing caches/prerequisites are blockers, not permission to install or silently switch to network recovery.

```powershell
$ErrorActionPreference = 'Stop'
$BuildSource = '6c6c0a5427dbc3bc59ea206bffdb232a195d41f5'
$PreparedNpmCache = 'D:\Documents\Nect-worktrees\codex-task5-layer-reorder-20261020-cache\npm'
$AppInputs = @('src','public','src-tauri','package.json','package-lock.json','index.html','vite.config.js')
git cat-file -e "$BuildSource^{commit}"
if ($LASTEXITCODE -ne 0) { throw 'Pinned source is unavailable' }
git diff --quiet $BuildSource -- @AppInputs
if ($LASTEXITCODE -ne 0) { throw 'Application inputs differ from the candidate' }
$UnexpectedInputs = @(git ls-files --others --exclude-standard -- @AppInputs)
if ($LASTEXITCODE -ne 0 -or $UnexpectedInputs.Count) { throw 'Untracked application inputs' }
foreach ($Tool in @('node','npm.cmd','cargo','rustc')) { Get-Command $Tool -ErrorAction Stop | Out-Null }
if (-not (Test-Path -LiteralPath $PreparedNpmCache)) { throw 'Prepared npm cache unavailable' }
$BuildRoot = Join-Path (Get-Location).Path ('test-results\windows-candidate-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))
New-Item -ItemType Directory -Path $BuildRoot -ErrorAction Stop | Out-Null
$env:TEMP = Join-Path $BuildRoot 'tmp'
$env:TMP = $env:TEMP
New-Item -ItemType Directory -Path $env:TEMP | Out-Null
$env:npm_config_cache = $PreparedNpmCache
$env:CARGO_TARGET_DIR = Join-Path $BuildRoot 'cargo-target'
$env:CARGO_NET_OFFLINE = 'true'
npm.cmd ci --offline --ignore-scripts --no-audit --no-fund *> (Join-Path $BuildRoot 'npm-ci.log')
if ($LASTEXITCODE -ne 0) { throw 'Locked npm preparation failed' }
npm.cmd run build *> (Join-Path $BuildRoot 'frontend-build.log')
if ($LASTEXITCODE -ne 0) { throw 'Frontend build failed; do not package an old dist' }
cargo build --release --target x86_64-pc-windows-msvc --features custom-protocol --locked --manifest-path src-tauri/Cargo.toml *> (Join-Path $BuildRoot 'native-build.log')
if ($LASTEXITCODE -ne 0) { throw 'Native build failed' }
$BuiltExe = Get-Item -LiteralPath (Join-Path $env:CARGO_TARGET_DIR 'x86_64-pc-windows-msvc\release\nect-svg-desktop.exe')
if ($BuiltExe.Length -le 0) { throw 'Empty executable' }
$Reader = [System.IO.BinaryReader]::new([System.IO.File]::OpenRead($BuiltExe.FullName))
try {
  if ($Reader.ReadUInt16() -ne 0x5A4D) { throw 'Missing MZ header' }
  $Reader.BaseStream.Position = 0x3c
  $PeOffset = $Reader.ReadInt32()
  $Reader.BaseStream.Position = $PeOffset
  if ($Reader.ReadUInt32() -ne 0x4550 -or $Reader.ReadUInt16() -ne 0x8664) { throw 'Not an AMD64 PE executable' }
} finally { $Reader.Dispose() }
$CandidateExe = Join-Path $BuildRoot 'Nect-test-6c6c0a5.exe'
Copy-Item -LiteralPath $BuiltExe.FullName -Destination $CandidateExe
$Hash = (Get-FileHash -LiteralPath $CandidateExe -Algorithm SHA256).Hash
if ($Hash -ne (Get-FileHash -LiteralPath $BuiltExe.FullName -Algorithm SHA256).Hash) { throw 'Artifact copy differs' }
$Manifest = Join-Path $BuildRoot 'candidate-manifest.json'
[ordered]@{
  application_source_commit = $BuildSource
  checkout_head = (git rev-parse HEAD).Trim()
  source_branch = (git branch --show-current).Trim()
  utc = [DateTime]::UtcNow.ToString('o')
  artifact = (Get-Item -LiteralPath $CandidateExe).Name
  bytes = (Get-Item -LiteralPath $CandidateExe).Length
  sha256 = $Hash
  node = (node -v)
  cargo = (cargo -V)
  rustc = (rustc -V)
  target = 'x86_64-pc-windows-msvc'
  frontend_build_exit = 0
  native_build_exit = 0
  pe_header = 'MZ / PE / AMD64 checked without launch'
  native_ui = 'NOT_RUN'
  distribution = 'NOT_DELIVERED'
} | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 -LiteralPath $Manifest
$Package = Join-Path $BuildRoot 'Nect-test-6c6c0a5.zip'
Compress-Archive -LiteralPath $CandidateExe,$Manifest -DestinationPath $Package
Get-Item -LiteralPath $CandidateExe,$Manifest,$Package | Select-Object FullName,Length
Get-FileHash -LiteralPath $Package -Algorithm SHA256
```

The existing scripts/build-native-candidate.ps1 is a fuller route with additional Node, Rust and font-smoke stages and a pinned Tauri CLI. It has not passed for this integration; its broader suites must retain their own results. The bounded route above follows the previously successful direct custom-protocol build, not a new packaging format. No installer or public release is requested.

Only after an actual EXE/package has been generated, measured, saved and its source/hash read back should the parent deliver a distinct test candidate. Preserve the old corrected executable. Artifact construction and static PE/hash checks do not prove native startup, native editing, IME or GUI acceptance.

## Targeted human retest after artifact creation

1. On a copy with three overlapping colored Layers, drag names above/below neighbors and to both list extremes. Insertion line, list and overlap must agree; one Undo/Redo restores the full order. Check ordinary click/rename and locked-source refusal.
2. Apply each dither to the original file copy and a moderate-spacing Text/vector fixture. Confirm marks, retained source/holes and clear rejection at excessive density; no partial authored changes. The original user's file/build are still unconfirmed until this exercise.
3. Drag/hold graph nodes and wires in the center, at the clamped top-left and at a scrolling edge. Pointer/snapping/wire feedback, cancellation and one-gesture history must remain usable; record actual feel/timing separately from source call counts.
4. Save As the same test artwork, restart, re-edit and export SVG/PNG. Record exact executable/source/checksum and preserve the original file. No full-Mission completion or new Image-output acceptance is implied.
