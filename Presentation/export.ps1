$ErrorActionPreference = 'Stop'
$folder = $PSScriptRoot
$deck = Join-Path $folder 'Arrakis_Presentation.pptx'
if (-not (Test-Path -LiteralPath $deck)) { throw 'Generate the PowerPoint first.' }
$preview = Join-Path $folder 'preview'
if (-not (Test-Path -LiteralPath $preview)) {
    New-Item -ItemType Directory -Path $preview | Out-Null
}
$wasRunning = @(Get-Process POWERPNT -ErrorAction SilentlyContinue).Count -gt 0
$application = $null
$presentation = $null
try {
    $application = New-Object -ComObject PowerPoint.Application
    $presentation = $application.Presentations.Open($deck, -1, 0, 0)
    if ($presentation.Slides.Count -ne 10) { throw 'The presentation must have exactly ten slides.' }
    $presentation.SaveAs((Join-Path $folder 'Arrakis_Presentation.pdf'), 32)
    $presentation.Export($preview, 'PNG', 1600, 900)
    $overflow = @()
    foreach ($slide in $presentation.Slides) {
        foreach ($shape in $slide.Shapes) {
            if ($shape.HasTextFrame -eq -1 -and $shape.TextFrame.HasText -eq -1) {
                $range = $shape.TextFrame2.TextRange
                if ($range.BoundHeight -gt $shape.Height + 2 -or $range.BoundWidth -gt $shape.Width + 2) {
                    $overflow += [PSCustomObject]@{
                        slide = $slide.SlideIndex
                        text = $range.Text
                        boxWidth = $shape.Width
                        textWidth = $range.BoundWidth
                        boxHeight = $shape.Height
                        textHeight = $range.BoundHeight
                    }
                }
            }
        }
    }
    ConvertTo-Json -InputObject @($overflow) -Depth 4 | Set-Content -LiteralPath (Join-Path $preview 'text_overflow.json') -Encoding utf8
    "Exported $($presentation.Slides.Count) slides to PDF and previews."
} finally {
    if ($null -ne $presentation) {
        $presentation.Close()
        [void][System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($presentation)
    }
    if ($null -ne $application) {
        if (-not $wasRunning) { $application.Quit() }
        [void][System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($application)
    }
    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
}
