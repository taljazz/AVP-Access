# Opens the PNG the png_small case wrote with the .NET image decoder and checks
# its pixels, so the encoder is proven against a real decoder and not only
# against the test's own reading of the format.
param([Parameter(Mandatory = $true)][string]$Path)

$ErrorActionPreference = 'Stop'
$failed = 0

function Check([bool]$condition, [string]$what) {
    if ($condition) { Write-Output "PASS: $what" }
    else { Write-Output "FAIL: $what"; $script:failed++ }
}

if (-not (Test-Path -LiteralPath $Path)) {
    Write-Output "FAIL: png_small did not write $Path"
    exit 1
}

Add-Type -AssemblyName System.Drawing
$image = [System.Drawing.Bitmap]::FromFile((Resolve-Path -LiteralPath $Path).Path)
try {
    Check ($image.Width -eq 3 -and $image.Height -eq 2) 'the .NET decoder reads a 3x2 image'

    # Written from bottom-up input: the top row is white, black, grey.
    $p = $image.GetPixel(0, 0)
    Check ($p.R -eq 255 -and $p.G -eq 255 -and $p.B -eq 255) 'top-left is white'
    $p = $image.GetPixel(2, 0)
    Check ($p.R -eq 128 -and $p.G -eq 128 -and $p.B -eq 128) 'top-right is grey'
    $p = $image.GetPixel(0, 1)
    Check ($p.R -eq 255 -and $p.G -eq 0 -and $p.B -eq 0) 'bottom-left is red'
    $p = $image.GetPixel(2, 1)
    Check ($p.R -eq 0 -and $p.G -eq 0 -and $p.B -eq 255) 'bottom-right is blue'
}
finally {
    $image.Dispose()
}

Write-Output "png_decode: 5 assertions, $failed failed"
if ($failed) { exit 1 }
exit 0
