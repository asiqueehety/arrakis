$ErrorActionPreference = 'Stop'
$reportDirectory = $PSScriptRoot
if (-not (Test-Path -LiteralPath $reportDirectory -PathType Container)) {
    throw 'The report directory does not exist.'
}
$outputDirectory = Join-Path $reportDirectory 'build'
if (-not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory | Out-Null
}
Push-Location -LiteralPath $reportDirectory
try {
    for ($pass = 1; $pass -le 2; $pass++) {
        & xelatex -no-shell-escape -interaction=nonstopmode -halt-on-error -file-line-error -output-directory=build -jobname=Arrakis_Report report.tex
        if ($LASTEXITCODE -ne 0) {
            throw "LaTeX compilation failed on pass $pass."
        }
    }
    Copy-Item -LiteralPath (Join-Path $outputDirectory 'Arrakis_Report.pdf') -Destination (Join-Path $reportDirectory 'Arrakis_Report.pdf') -Force
} finally {
    Pop-Location
}
