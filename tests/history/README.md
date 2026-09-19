# Message history fixture

This fixture compiles the real `src/avp/messagehistory.c` by including it in `messagehistory_test.c`. It uses the project engine headers and mocks only the game state, text lookup, onscreen message hook, frame time, and save allocator boundaries.

Run from the repository root in a Windows command prompt or PowerShell session with Visual Studio C++ tools available:

```bat
call tests\history\run_tests.bat
```

The batch runner initializes the shared MSVC environment through `tools/env.bat`, builds the fixture, runs it, and removes generated executable/object files. It does not launch the game.

There are 21 checks. Invalid timer/count/cursor/payload checks also compare the
existing history state before another display call, so that formatting a message
cannot mask an incorrectly accepted timer. Actual NVDA output and live campaign
save/load behavior remain separate from these fixture results.
