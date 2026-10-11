param([switch]$Recapture)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $PSScriptRoot)) { throw 'Presentation folder missing.' }
if ($Recapture) {
    $temporaryRoot = 'C:/Users/asiqu/AppData/Local/Temp/kilo'
    if (-not (Test-Path -LiteralPath $temporaryRoot)) { throw 'Temporary build parent missing.' }
    $captureBuild = Join-Path $temporaryRoot 'arrakis-presentation-capture'
    & cmake -S (Join-Path $PSScriptRoot 'capture') -B $captureBuild -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE -ne 0) { throw 'Capture configuration failed.' }
    & cmake --build $captureBuild --config Release
    if ($LASTEXITCODE -ne 0) { throw 'Capture build failed.' }
    & python (Join-Path $PSScriptRoot 'capture_assets.py') --capture-exe (Join-Path $captureBuild 'Release/Capture.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Figure capture failed.' }
}
& python (Join-Path $PSScriptRoot 'generate_presentation.py')
if ($LASTEXITCODE -ne 0) { throw 'Slide generation failed.' }
& (Join-Path $PSScriptRoot 'export.ps1')
& python (Join-Path $PSScriptRoot 'verify_presentation.py')
if ($LASTEXITCODE -ne 0) { throw 'Presentation checks failed.' }
