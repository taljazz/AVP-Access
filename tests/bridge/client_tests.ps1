param([Parameter(Mandatory=$true)][string]$RuntimeExe)
$ErrorActionPreference='Stop'
$client=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../tools/bridge.ps1'))
$root=Join-Path ([IO.Path]::GetTempPath()) ('avp-client-test-'+[Guid]::NewGuid().ToString('N'))
$dir=Join-Path $root 'bridge'
$fixture=$null
$checks=0
function Check([bool]$condition,[string]$description) {
    if (!$condition) { throw "FAIL: $description" }
    $script:checks++; Write-Output "PASS: $description"
}
function Send([string]$command,[int]$timeout=5) {
    & $client -Directory $dir -Command $command -TimeoutSeconds $timeout | ConvertFrom-Json
}
try {
    New-Item -ItemType Directory -Path $root | Out-Null
    Copy-Item -LiteralPath $RuntimeExe -Destination (Join-Path $root 'avp.exe')
    $fixture=Start-Process -FilePath (Join-Path $root 'avp.exe') -ArgumentList ('client_server "'+$dir+'"') -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $root 'stdout.log') -RedirectStandardError (Join-Path $root 'stderr.log')
    $deadline=[DateTime]::UtcNow.AddSeconds(5)
    while (!(Test-Path -LiteralPath (Join-Path $dir 'ready.json'))) {
        if ($fixture.HasExited -or [DateTime]::UtcNow -gt $deadline) { throw 'Fixture did not start' }
        Start-Sleep -Milliseconds 25
    }
    $reply=Send 'state'; Check ($reply.seq -eq 1 -and $reply.result -eq 'ok') 'state uses first matching sequence'
    $reply=Send 'tap i'; Check ($reply.seq -eq 2 -and $reply.result -eq 'ok') 'ASCII i is accepted without Unicode case-folding'
    $rejected=$false
    try { Send ('tap '+[char]0x00e9) | Out-Null } catch { $rejected=$_.Exception.Message -like 'Use one ASCII*' }
    Check $rejected 'non-ASCII input is rejected before publication'
    $rejected=$false
    try { Send "tap w`nquit" | Out-Null } catch { $rejected=$_.Exception.Message -like 'Use one ASCII*' }
    Check $rejected 'multiline input is rejected before publication'
    $lock=[IO.File]::Open((Join-Path $dir 'client.lock'),[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    try {
        $rejected=$false
        try { Send 'state' | Out-Null } catch { $rejected=$true }
        Check $rejected 'another client cannot enter an active transaction'
    } finally { $lock.Dispose() }
    $timedOut=$false
    try { Send 'run 2200' 1 | Out-Null } catch { $timedOut=$_.Exception.Message -like 'Timed out waiting*' }
    Check $timedOut 'timeout reports an uncertain command without resending'
    $rejected=$false
    try { Send 'tap w' | Out-Null } catch { $rejected=$_.Exception.Message -like 'The previous command has not replied*' }
    Check $rejected 'consumed but unfinished command prevents a second submission'
    Start-Sleep -Milliseconds 1500
    $reply=Send 'state'; Check ($reply.seq -eq 4) 'completed timed-out command allows next unique sequence'
    $rejected=$false
    try { Send 'tap a b c d e f g h i' | Out-Null } catch { $rejected=$_.Exception.Message -like "Bridge command returned 'error'*" }
    Check $rejected 'parse error returns its matching sequence instead of timing out'
    $reply=Send 'quit'; Check ($reply.seq -eq 6 -and $reply.result -eq 'ok') 'quit returns its final matching response'
    Check ($fixture.WaitForExit(3000)) 'fixture exits after quit'
    Check (!(Test-Path -LiteralPath (Join-Path $dir 'ready.json'))) 'normal shutdown removes readiness marker'
    Write-Output "Bridge client: $checks checks, 0 failures"
}
finally {
    if ($fixture) { $fixture.Refresh(); if (!$fixture.HasExited) { $fixture.Kill(); $fixture.WaitForExit() }; $fixture.Dispose() }
    # Delete only the unique, newly created test directory, using one shell.
    $resolved=[IO.Path]::GetFullPath($root)
    $tempRoot=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
    if ($resolved.StartsWith($tempRoot,[StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($resolved) -like 'avp-client-test-*' -and (Test-Path -LiteralPath $resolved)) {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
