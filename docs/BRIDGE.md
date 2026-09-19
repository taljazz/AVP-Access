# Local play bridge

The optional bridge lets a developer inspect screenshots, send bounded input,
and correlate speech requests with accessibility sound starts. Normal launches
are unaffected. Use one game process and one bridge directory per session.

From PowerShell in the repository directory:

```powershell
.\tools\run.bat -w --skip-intro --bridge
```

This mode suppresses screen-reader output and mutes game sound. Add
`--bridge-audible` for a listening session. In a second PowerShell window:

```powershell
.\tools\bridge.ps1 -Command 'shot'
.\tools\bridge.ps1 -Command 'tap r'
.\tools\bridge.ps1 -Command 'run 1200 shot'
.\tools\bridge.ps1 -Command 'hold w 200 shot'
.\tools\bridge.ps1 -Command 'turn right 30 shot'
.\tools\bridge.ps1 -Command 'quit'
```

The example movement keys assume the Marine default bindings. Commands operate
through engine input; they do not infer obstacles or select safe routes. Inspect
the returned screenshot before choosing movement. `turn` uses current turn
bindings and can report stalled, timeout or wrong_direction as well as ok.

`--bridge-dir "C:\path\session"` selects a different directory; pass the same
directory to the client with `-Directory`. The default is `build\bridge` beside
the executable. Output is text JSON with a sequence number, current context,
player state when available, events, and an optional absolute screenshot path.

Useful commands: `state`, `shot`, `tap enter`, `tap pad_r3`, `tap pad_right`,
`hold w 200`, `run 1200`, `turn left 30`, `sounds all`, `sounds access`,
`realtime`, `step`, and `quit`. Inputs can combine up to four keys with `+`.
Timed commands accept up to 60,000 ms. `shot` can follow movement commands.

Gameplay defaults to fixed steps of approximately 1/30 second and waits between
commands. Menus/loading continue on wall time. `realtime` allows gameplay to run
freely; `step` restores held time. The bridge does not capture the physical mouse.

## Logs and timing

`events.jsonl` records speech requests (including when output is suppressed) and
tagged sound starts. `t` is milliseconds on the bridge clock: simulation time in
held gameplay, wall-clock progression elsewhere. `frame` is the engine frame
counter and can reset at level changes. Cue records carry source, sound/sample,
volume, pitch, loop flag, range and, when available, world position and horizontal
distance/bearing relative to the player. `cue_dropped` reports a tagged request
that did not produce an accepted sound start. `sounds all` includes other sounds
started through the engine sound API.

These timestamps are not measured speaker or NVDA onset times. The log does not
yet cover sound completion/stop events or FFmpeg voiceovers. Long event fields
can be truncated and marked as such; replies hold the latest 512 events and report
overflow. The on-disk event log normally retains events beyond the reply ring.

## Failure handling

The client serializes send/wait transactions with `client.lock`. A timed-out
command may still complete: the client retains `client-pending.json` and refuses
to submit another command until the corresponding reply exists. It never
automatically repeats movement. Check the existing reply and game process before
retrying; restarting the game establishes a new session.

If reply publication fails, the game releases injected input, retains the reply
and undelivered events, and retries once per second before accepting more commands.
The prior complete reply remains in place. Startup fails with an error if its
essential files cannot be created; it does not report readiness and freeze an
unreachable game. Normal exit removes `ready.json`; forced termination can leave
a stale marker, so the client also checks the process.

Core tests: `tests\bridge\run_tests.bat`.
Runtime/client tests: `tests\bridge\run_runtime_tests.bat`.
The latter uses actual SDL filesystem operations and Windows locks, with mocked
engine state; its child process named avp.exe is a headless fixture, not the game.
