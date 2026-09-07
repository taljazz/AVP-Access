param([Parameter(Mandatory=$true)][string]$Source, [Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
$text = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $Source))
function Get-Function([string]$signature) {
    $match = [regex]::Match($text, '(?m)^' + [regex]::Escape($signature) + '\s*\{')
    if (!$match.Success) { throw "Missing source function: $signature" }
    $start = $text.IndexOf('{', $match.Index)
    $depth = 1
    $end = $start + 1
    while ($depth -gt 0 -and $end -lt $text.Length) {
        if ($text[$end] -eq '{') { $depth++ }
        if ($text[$end] -eq '}') { $depth-- }
        $end++
    }
    if ($depth -ne 0) { throw "Unbalanced source function: $signature" }
    $text.Substring($match.Index, $end - $match.Index)
}
$headerEnd = $text.IndexOf('extern int ScanDrawMode;')
$stateStart = $text.IndexOf('/* motion tracker info */')
$stateEnd = $text.IndexOf('int predHUDSoundHandle=')
$speed = [regex]::Match($text, '(?m)^int MotionTrackerSpeed[^\n]*\nint MotionTrackerVolume[^\n]*\n#define MOTIONTRACKERVOLUME[^\n]*')
$gate = [regex]::Match($text, '(?s)trackerActive = AvP\.PlayerType==I_Marine.*?if \(!trackerActive\) AccTracker_ResetHUD\(\);')
if ($headerEnd -lt 0 -or $stateStart -lt 0 -or $stateEnd -lt $stateStart -or !$speed.Success -or !$gate.Success) {
    throw 'HUD fixture source boundaries changed; review the extractor before running tests.'
}
$pieces = @(
    '/* Generated from actual hud.c; do not edit or check in. */',
    $text.Substring(0,$headerEnd),
    'extern DISPLAYBLOCK *Player; extern int NumActiveStBlocks, NormalFrameTime;',
    'extern STRATEGYBLOCK *ActiveStBlockList[maxstblocks];',
    'char ValueOfHUDDigit[MAX_NO_OF_MARINE_HUD_DIGITS];',
    $text.Substring($stateStart,$stateEnd-$stateStart),
    $speed.Value,
    'int Fast2dMagnitude(int dx,int dy);',
    'static int DoMotionTrackerBlips(VECTORCH *nearestPosition);',
    (Get-Function 'void AccTracker_ResetHUD(void)'),
    (Get-Function 'static void DoMotionTracker(void)'),
    (Get-Function 'int ObjectShouldAppearOnMotionTracker(STRATEGYBLOCK *sbPtr)'),
    (Get-Function 'static int DoMotionTrackerBlips(VECTORCH *nearestPosition)'),
    (Get-Function 'int Fast2dMagnitude(int dx, int dy)'),
    'static int TestHUDTrackerEligibility(PLAYER_STATUS *playerStatusPtr) { int trackerActive;',
    $gate.Value,
    'return trackerActive; }'
)
[IO.File]::WriteAllText($Output, ($pieces -join "`n"))
