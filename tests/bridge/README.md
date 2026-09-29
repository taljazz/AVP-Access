# Bridge regression checks

`run_tests.bat` compiles the real engine-free core. It runs 163 C assertions and
five independent PNG decoder checks for commands, bounded input, turning, JSON,
relative positions and images.

`run_runtime_tests.bat` compiles the real bridge integration against mocked engine
state/input/speech/screenshots but real SDL filesystem and Windows file locks.
Its 38 checks cover pending reply recovery, release on failure, preserved events,
blocked next commands, locked command deletion, locked reply replacement, rejected
oversized/binary input, startup failure, readiness cleanup, held-time close, and
gameplay-only map export, output path, reply detail and exporter failure handling.

It then runs 12 PowerShell client checks against a headless helper named avp.exe
(compiled from that same fixture). These cover sequencing, ASCII validation,
client locking, timeout protection, correlated parse errors and quit. There is
no retail game, graphics window, audio backend or screen-reader session involved.
The helper and its files are created in a unique temporary directory and removed.

These checks do not replace live gameplay, listening, controller or movie tests.
