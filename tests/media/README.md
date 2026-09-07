# Menu music regression checks

Run from the repository root in a Windows command prompt:

```bat
tests\media\run_tests.bat
```

The runner uses `tools\env.bat` to find MSVC and SDL3. Python must be on PATH,
or set `AVP_TEST_PYTHON` to the full path of a Python executable.

`extract_menu.py` extracts the current production menu playback functions from
`acc_media.c` and `avp_intro.cpp`. The first test compiles those functions against
a small fake stream backend. Its 22 checks cover the original menu asset, audio
buffer refilling before completion checks, repeat and stop behavior, numbered
gameplay tracks, missing files, and delayed audio initialization. Production
logic is extracted afresh on each run rather than copied into the test.

The second test compiles the complete production `acc_media.c` without FFmpeg,
then calls its public API to verify that the optional dependency fallback links
and safely returns unavailable results.

These tests open no game window, play no audio, and read no profiles or saves.
Decoder behavior and audible playback still require a live game check.

To check a staged source tree, set `AVP_TEST_PROJECT` to the real repository
(for its build environment) and pass the staged root to the runner:

```bat
set "AVP_TEST_PROJECT=C:\path\to\NakedAVP"
set "AVP_TEST_PYTHON=C:\path\to\python.exe"
run_tests.bat "C:\path\to\staged"
```

Generated headers, object files, and executables are ignored by Git.
