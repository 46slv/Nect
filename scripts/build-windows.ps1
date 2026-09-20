param(
    [string]$BoostRoot = "$PSScriptRoot/../build/deps/boost_1_85_0",
    [string]$QtRoot = "$PSScriptRoot/../build/deps/6.5.3/msvc2019_64",
    [string]$BuildDirectory = "$PSScriptRoot/../build",
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath "$PSScriptRoot/..").Path
$boost = (Resolve-Path -LiteralPath $BoostRoot).Path
$qt = (Resolve-Path -LiteralPath $QtRoot).Path
cmake -S $repo -B $BuildDirectory -G 'Visual Studio 16 2019' -A x64 -DBUILD_TESTING=ON -DNECT_DESKTOP=ON "-DBOOST_INCLUDE_DIR=$boost" "-DCMAKE_PREFIX_PATH=$qt"
if($LASTEXITCODE -ne 0) { throw 'Configure failed' }
cmake --build $BuildDirectory --config Release -j 8
if($LASTEXITCODE -ne 0) { throw 'Build failed' }
$output = Join-Path $BuildDirectory 'Release'
& "$qt/bin/windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw "$output/nect_desktop.exe"
if($LASTEXITCODE -ne 0) { throw 'Qt local runtime deployment failed' }
Copy-Item -LiteralPath "$qt/bin/Qt6Test.dll" -Destination "$output/Qt6Test.dll"
Copy-Item -LiteralPath "$qt/plugins/platforms/qoffscreen.dll" -Destination "$output/platforms/qoffscreen.dll"
if(!$SkipTests) {
    ctest --test-dir $BuildDirectory -C Release --output-on-failure --timeout 60
    if($LASTEXITCODE -ne 0) { throw 'Tests failed' }
}
Write-Output "Desktop: $output/nect_desktop.exe"
