param([string]$Cargo = 'C:\Users\TWRT\.cargo\bin\cargo.exe')
$ErrorActionPreference = 'SilentlyContinue'
$result = [ordered]@{ platform = $env:OS; cargo = $null; rustup = $null; rustc = $null; node = $null; npm = $null; windowsSdk26100 = $false; webView2 = $false; msvc = 'NOT_RESOLVED_FROM_CURRENT_SHELL' }
if (Test-Path $Cargo) { $result.cargo = (& $Cargo --version).Trim() }
$rustup = Join-Path (Split-Path $Cargo) 'rustup.exe'
if (Test-Path $rustup) { $result.rustup = (& $rustup --version | Select-Object -First 1).Trim(); $result.rustc = (& $rustup run stable rustc --version | Select-Object -First 1).Trim() }
$nodeCmd = Get-Command node; if ($nodeCmd) { $result.node = (& node --version).Trim() }
$npmCmd = Get-Command npm; if ($npmCmd) { $result.npm = (& npm --version).Trim() }
$result.windowsSdk26100 = Test-Path 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0'
$result.webView2 = [bool](Get-ChildItem 'C:\Program Files (x86)\Microsoft\EdgeWebView\Application\*\msedgewebview2.exe')
$result | ConvertTo-Json
if (-not $result.cargo -or -not $result.rustup -or -not $result.rustc -or -not $result.node -or -not $result.npm -or -not $result.windowsSdk26100 -or -not $result.webView2) { exit 1 }
