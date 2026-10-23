param(
    [string]$SourceCommit = '7991d86f66db0dd3587eddd34acb604d514eb73e'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$desktop = Split-Path $PSScriptRoot -Parent
Push-Location $desktop
try {
    $origin = git remote get-url origin
    if ($LASTEXITCODE -ne 0 -or $origin -ne 'https://github.com/46slv/Nect.git') {
        throw 'Expected origin https://github.com/46slv/Nect.git'
    }
    git cat-file -e "$SourceCommit^{commit}"
    if ($LASTEXITCODE -ne 0) { throw 'Source commit is unavailable' }
    # Documentation and recovery scripts may advance without changing app inputs.
    $inputs = @('src', 'public', 'src-tauri', 'package.json', 'package-lock.json', 'index.html', 'vite.config.js')
    git diff --quiet $SourceCommit -- @inputs
    if ($LASTEXITCODE -ne 0) { throw 'Application inputs differ from the declared source commit' }
    $untracked = @(git ls-files --others --exclude-standard -- @inputs)
    if ($LASTEXITCODE -ne 0 -or $untracked.Count) { throw 'Untracked application inputs exist' }
    $env:Path = "$env:USERPROFILE\.cargo\bin;$env:Path"
    foreach ($tool in @('cargo', 'rustc', 'node', 'npm.cmd', 'npx.cmd')) {
        Get-Command $tool -ErrorAction Stop | Out-Null
    }
    New-Item -ItemType Directory -Force test-results | Out-Null
    function Invoke-Logged([string]$Name, [scriptblock]$Action) {
        & $Action *> "test-results/$Name.log"
        if ($LASTEXITCODE -ne 0) { throw "$Name failed; inspect test-results/$Name.log" }
    }
    Invoke-Logged 'npm-ci' { npm.cmd ci }
    Invoke-Logged 'focused-node' {
        node --test tests/named-presets.test.js tests/authored-value-expressions.test.js tests/parameter-reference-expressions.test.js tests/context-menu-session.test.js tests/node-context-menu-dom.test.js tests/recovery-autosave.test.js tests/document-lifecycle.test.js tests/text-tool-dom.test.js tests/handle-polar.test.js tests/node-catalog.test.js tests/node-picker-dom.test.js
    }
    Invoke-Logged 'frontend-build' { npm.cmd run build }
    Invoke-Logged 'native-rust' { cargo test --locked --manifest-path src-tauri/Cargo.toml -- --nocapture }
    Invoke-Logged 'native-font-smoke' { node tests/native-font-smoke.js }
    Invoke-Logged 'native-release-build' { npx.cmd --yes @tauri-apps/cli@2.12.3 build --no-bundle -- --locked }
    $exe = Get-Item -LiteralPath 'src-tauri/target/release/nect-svg-desktop.exe'
    [ordered]@{
        application_source_commit = $SourceCommit
        checkout_head = (git rev-parse HEAD)
        origin = $origin
        utc = [DateTime]::UtcNow.ToString('o')
        rustc = (rustc -V)
        cargo = (cargo -V)
        node = (node -v)
        npm = (npm.cmd -v)
        tauri_cli = '2.12.3'
        standalone = @{ path = $exe.FullName; bytes = $exe.Length; sha256 = (Get-FileHash $exe.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
        checks = 'See stage logs; Rust tests can report explicit unavailable font skips'
        native_ui = 'NOT RUN: requires a working native UI automation runtime or human exercise'
        excluded = 'No installer, release, merge, physical IME or f54505a inclusion claim'
    } | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 test-results/native-build.json
    Write-Output "Built $($exe.FullName); evidence: test-results/native-build.json"
} finally { Pop-Location }
