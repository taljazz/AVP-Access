# Motion-tracker HUD regression fixture

Run `tests\tracker\run_hud_tests.bat` from a Visual Studio command prompt, or let the runner initialize Visual Studio using `tools\env.bat`.

The runner compiles the actual tracker functions and eligibility statements extracted from `src\avp\hud.c`, using the real engine headers and trigonometry tables. It mocks rendering and sound outputs, so it neither launches nor builds the game. Generated source and compiler outputs are ignored by Git. The extractor fails if its source boundaries change.

The 45 checks cover nearest-contact coordinates and tie order, range and front-half filtering, object exclusions, sweep crossings, capacity, the three existing beep bands, one beep per locked sweep, unchanged scan clicks, post-fade contact publication, complete resets and gameplay eligibility. The source gate is extracted into a test wrapper; the fixture does not invoke all of `MaintainHUD`, the main loop or world teardown. Those integrations still require a game build and live pause/restart checks.

An optional first argument selects another `hud.c`. When testing staged source outside the repository, set `AVP_TEST_PROJECT` to the real project directory and, if needed, `TEST_ACCESS_INCLUDE` to the directory containing the staged `acc_tracker.h`.
