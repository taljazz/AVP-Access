# Objective tests

Run `run_tests.bat` from this directory. It compiles the real objective readout
against mocked objective enumeration, strings and speech. No game data, audio
device or controller is needed; MSVC is located through `tools/env.bat`.

The 18 checks cover empty lists, achievement states, blank descriptions, invalid
indices, cycling, shrinking lists, resetting and interruption policy. They do not
exercise the C++ mission enumeration, game-level content or actual NVDA output.
