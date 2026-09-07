# Compile the actual menu playback control functions against a small fake backend.
#
# PowerShell rather than Python: every other suite here needs only MSVC, and
# Windows always has PowerShell. Requiring Python meant this suite could not run
# on a machine that had everything else it needed.
#
# Usage: extract_menu.ps1 <source root> <output header>

param(
    [Parameter(Mandatory = $true)][string]$Root,
    [Parameter(Mandatory = $true)][string]$Output
)

$ErrorActionPreference = 'Stop'

function Get-Function {
    param([string]$Source, [string]$Name)

    $pattern = '^(?:int|void)\s+' + [regex]::Escape($Name) + '\([^\n]*\)\s*\{'
    $match = [regex]::Match($Source, $pattern, 'Multiline')
    if (-not $match.Success) {
        throw "Missing production function: $Name"
    }

    # Brace-match from just past the opening brace.
    $depth = 1
    $cursor = $match.Index + $match.Length
    while ($depth -gt 0) {
        if ($cursor -ge $Source.Length) {
            throw "Unbalanced braces in $Name"
        }
        $c = $Source[$cursor]
        if ($c -eq '{') { $depth++ }
        elseif ($c -eq '}') { $depth-- }
        $cursor++
    }

    return $Source.Substring($match.Index, $cursor - $match.Index)
}

$rootPath = (Resolve-Path $Root).Path
$media = Get-Content -Raw -LiteralPath (Join-Path $rootPath 'src/access/acc_media.c')
$intro = Get-Content -Raw -LiteralPath (Join-Path $rootPath 'src/avp/win95/frontend/avp_intro.cpp')

$parts = @(
    (Get-Function $media 'AccMedia_PlayMenuMusic'),
    (Get-Function $media 'AccMedia_PlayTrack'),
    (Get-Function $intro 'StartMenuMusic'),
    (Get-Function $intro 'PlayMenuMusic'),
    (Get-Function $intro 'EndMenuMusic')
)

Set-Content -LiteralPath $Output -Value (($parts -join "`n`n") + "`n") -Encoding utf8
