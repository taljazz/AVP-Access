param(
    [Parameter(Mandatory=$true)][string]$Command,
    [string]$Directory = (Join-Path $PSScriptRoot '../../build/bridge'),
    [ValidateRange(1,300)][int]$TimeoutSeconds = 75
)
$ErrorActionPreference = 'Stop'
$Directory = [IO.Path]::GetFullPath($Directory)
if ($Command.Length -gt 400 -or $Command -cmatch '[\x00-\x1f\x7f-\uffff]') {
    throw 'Use one ASCII command line, at most 400 characters, without a sequence number.'
}

$lock = $null
$temporary = $null
$sent = $false
$pendingWritten = $false
try {
    # Cooperating clients serialize the complete send/wait transaction. The empty
    # lock file is retained: deleting it on release introduces a lock-file race.
    $lock = [IO.File]::Open((Join-Path $Directory 'client.lock'),
        [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $readyPath = Join-Path $Directory 'ready.json'
    $ready = [IO.File]::ReadAllText($readyPath) | ConvertFrom-Json
    $gameProcess = Get-Process -Id $ready.pid -ErrorAction Stop
    if ($gameProcess.ProcessName -ne 'avp') { throw 'The ready file does not identify a running AVP process.' }
    $readyStamp = [IO.File]::GetLastWriteTimeUtc($readyPath)
    $commandPath = Join-Path $Directory 'command.txt'
    $replyPath = Join-Path $Directory 'reply.json'
    if (Test-Path -LiteralPath $commandPath) {
        throw 'A command is already pending. Wait for it to finish; it has not been overwritten.'
    }
    # If an earlier client timed out, the command may already be consumed but
    # still running. Retain its sequence in a sidecar until a reply confirms it.
    $pendingPath = Join-Path $Directory 'client-pending.json'
    $previous = $null
    if (Test-Path -LiteralPath $replyPath) { $previous = [IO.File]::ReadAllText($replyPath) | ConvertFrom-Json }
    if (Test-Path -LiteralPath $pendingPath) {
        $pending = [IO.File]::ReadAllText($pendingPath) | ConvertFrom-Json
        if ($pending.pid -eq $ready.pid -and $pending.stamp -eq $readyStamp.Ticks.ToString() -and
            (!$previous -or $previous.seq -ne $pending.seq)) {
            throw 'The previous command has not replied. It may still complete; no new command was sent.'
        }
    }
    $sequence = if ($previous) { [long]$previous.seq + 1 } else { 1 }
    if ($sequence -gt [int]::MaxValue) { throw 'Sequence limit reached; restart the bridge session.' }
    $pending = @{ pid=$ready.pid; stamp=$readyStamp.Ticks.ToString(); seq=$sequence }
    [IO.File]::WriteAllText($pendingPath, ($pending | ConvertTo-Json -Compress))
    $pendingWritten = $true
    $temporary = Join-Path $Directory ('client-' + [Guid]::NewGuid().ToString('N') + '.tmp')
    [IO.File]::WriteAllText($temporary, "$sequence $Command", [Text.Encoding]::ASCII)
    [IO.File]::Move($temporary, $commandPath)
    $sent = $true
    $temporary = $null
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        # Read a final quit reply before checking whether the process exited.
        $reply = $null
        try { $reply = [IO.File]::ReadAllText($replyPath) | ConvertFrom-Json } catch { }
        if ($reply -and $reply.seq -eq $sequence) {
            Remove-Item -LiteralPath $pendingPath
            $reply | ConvertTo-Json -Depth 12
            if ($reply.result -ne 'ok') { throw "Bridge command returned '$($reply.result)': $($reply.error)" }
            return
        }
        $gameProcess.Refresh()
        if ($gameProcess.HasExited) { throw 'AVP exited before publishing the matching reply.' }
        if (!(Test-Path -LiteralPath $readyPath) -or [IO.File]::GetLastWriteTimeUtc($readyPath) -ne $readyStamp) {
            throw 'The bridge session changed while waiting. No command was retried.'
        }
        Start-Sleep -Milliseconds 50
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for command $sequence. It may still complete; do not resend it blindly."
}
finally {
    if ($pendingWritten -and !$sent -and (Test-Path -LiteralPath $pendingPath)) { Remove-Item -LiteralPath $pendingPath }
    if ($temporary -and (Test-Path -LiteralPath $temporary)) { Remove-Item -LiteralPath $temporary }
    if ($lock) { $lock.Dispose() }
}
