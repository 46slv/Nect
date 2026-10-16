param(
    [string]$QtRoot = 'D:/Documents/Nect/build/deps/6.5.3/msvc2019_64',
    [string]$BoostRoot = 'D:/Documents/Nect/build/deps/boost_1_85_0',
    [string]$BuildDirectory = "$PSScriptRoot/../build-studio"
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath "$PSScriptRoot/..").Path
# Serialize with existing Nect Windows builds; never acquire an abandoned lease.
$buildMutex = [Threading.Mutex]::new($false, 'Local\NectBuild')
$acquired = $false
try {
    try { $acquired = $buildMutex.WaitOne(0) }
    catch [Threading.AbandonedMutexException] { $buildMutex.ReleaseMutex(); throw 'NectBuild was abandoned; inspect the prior build before retrying.' }
    if (!$acquired) { throw 'NectBuild is busy. No build started.' }
    cmake -S $repo -B $BuildDirectory -G 'Visual Studio 16 2019' -A x64 -DBUILD_TESTING=ON -DNECT_STUDIO=ON -DNECT_DESKTOP=OFF "-DBOOST_INCLUDE_DIR=$BoostRoot" "-DCMAKE_PREFIX_PATH=$QtRoot"
    if ($LASTEXITCODE -ne 0) { throw 'Studio configure failed' }
    cmake --build $BuildDirectory --config Release --target nect_studio studio_workflow_tests -- /m:2 /nodeReuse:false
    if ($LASTEXITCODE -ne 0) { throw 'Studio build failed' }
    $output = Join-Path $BuildDirectory 'Release'
    & "$QtRoot/bin/windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw "$output/nect_studio.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed' }
    Copy-Item -LiteralPath "$QtRoot/bin/Qt6Test.dll" -Destination "$output/Qt6Test.dll"
    Copy-Item -LiteralPath "$QtRoot/plugins/platforms/qoffscreen.dll" -Destination "$output/platforms/qoffscreen.dll"
    Write-Output "Studio: $output/nect_studio.exe"
} finally {
    if ($acquired) { $buildMutex.ReleaseMutex() }
    $buildMutex.Dispose()
}
