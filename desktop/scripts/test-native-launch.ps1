param(
    [string]$Executable = (Join-Path (Split-Path $PSScriptRoot -Parent) 'src-tauri\target\release\nect-svg-desktop.exe'),
    [int]$ObservationSeconds = 15
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($ObservationSeconds -lt 5 -or $ObservationSeconds -gt 45) { throw 'Observation must be 5-45 seconds' }
$exe = Get-Item -LiteralPath $Executable
$root = Join-Path $exe.DirectoryName ('nect-svg-acceptance-' + [Guid]::NewGuid().ToString('N'))
$prior = [Environment]::GetEnvironmentVariable('NECT_SVG_ACCEPTANCE_ROOT', 'Process')
$process = $null
$receipt = [ordered]@{
    utc = [DateTime]::UtcNow.ToString('o')
    executable = $exe.FullName
    sha256 = (Get-FileHash $exe.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    acceptance_root = $root
    observation_seconds = $ObservationSeconds
    process_smoke = 'FAIL'
    native_ui = 'NOT RUN: process/window metadata does not verify rendered content or interaction'
}
try {
    $env:NECT_SVG_ACCEPTANCE_ROOT = $root
    $process = Start-Process -FilePath $exe.FullName -WorkingDirectory $exe.DirectoryName -WindowStyle Hidden -PassThru
    Start-Sleep -Seconds $ObservationSeconds
    $process.Refresh()
    $receipt.pid = $process.Id
    $receipt.has_exited = $process.HasExited
    if ($process.HasExited) {
        $receipt.exit_code = $process.ExitCode
        throw 'Native process exited during observation'
    }
    $receipt.main_window_title = $process.MainWindowTitle
    $receipt.main_window_handle = $process.MainWindowHandle.ToInt64()
    $receipt.responding = $process.Responding
    $receipt.acceptance_profile_created = Test-Path -LiteralPath (Join-Path $root 'webview2')
    if (-not $receipt.acceptance_profile_created) { throw 'Isolated profile was not created' }
    if (-not $receipt.responding -or $receipt.main_window_handle -eq 0) { throw 'No responding native window' }
    $receipt.process_smoke = 'PASS'
} finally {
    [Environment]::SetEnvironmentVariable('NECT_SVG_ACCEPTANCE_ROOT', $prior, 'Process')
    # Only the disposable process created here can be closed or stopped.
    if ($process -and -not $process.HasExited) {
        $receipt.close_requested = $process.CloseMainWindow()
        $receipt.closed_within_5_seconds = $process.WaitForExit(5000)
        if (-not $receipt.closed_within_5_seconds) {
            $process.Kill()
            $process.WaitForExit()
            $receipt.forced_disposable_process_stop = $true
        }
    }
    $results = Join-Path (Split-Path $PSScriptRoot -Parent) 'test-results'
    New-Item -ItemType Directory -Force $results | Out-Null
    $receipt | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $results 'native-launch.json')
    $receipt | ConvertTo-Json -Depth 4
}
