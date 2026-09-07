# Controller input regression checks

Run `run_tests.bat` from a Windows command prompt. The runner uses `tools/env.bat`
to find Visual Studio, the repository headers, and the installed SDL3 SDK.

The harness compiles the actual `src/access/acc_pad.c` with mocked SDL functions.
It does not link the SDL DLL, open a window, run the game, or access a controller.
Each scenario runs in a separate process, so module static state starts clean.
Exit status is 0 for success, 1 for regression failures, and 2 for setup or
compilation failure.

To test a staged source file, use:

```bat
run_tests.bat fixed "path\to\acc_pad.c"
```

If the harness is outside the repository, set `AVP_TEST_PROJECT` to the repository
root before running it. SDK paths and Visual Studio locations are obtained from
the project's environment script; the harness contains no machine-specific paths.

For before/after verification against an older source snapshot:

```bat
run_tests.bat baseline "path\to\older\acc_pad.c"
```

Baseline mode omits calls to new APIs and skips the disconnect/reconnect scenario
because that API did not exist. Supply any historical source externally; do not
commit a duplicate baseline source file.

Coverage includes menu D-pad press/hold/release, keyboard preservation, the
any-key edge used by loading-screen waits, held B, overlapping keyboard/pad
releases in both directions, combined Start/B state, button binding without menu
aliases, and disconnect/reconnect with physical keys still held. Gameplay bridge
checks cover both sticks (neutral, dead-zone edges, half/full deflection, release),
trigger thresholds and independent press edges, all 16 button slots, and clearing
menu aliases when gameplay starts. Start still supplies Escape for pause.

There are 240 assertions across 13 scenarios. The separate `tests/gameplay`
fixture exercises actual gameplay requests and Marine profile migration. SDL event
delivery, menu rendering, and game-world responses still need live checks.
