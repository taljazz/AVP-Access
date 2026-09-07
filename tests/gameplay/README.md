# Gameplay input regression fixture

Run `run_input_tests.bat` from a Windows command prompt. It uses `tools/env.bat`
for the compiler and SDKs and compiles the actual `usr_io.c` input implementation.
The fixture supplies minimal engine/SDL stubs and a player block; it does not
build or launch the game, access game data, or require a controller.

An optional first argument selects a staged `usr_io.c`. If these tests are outside
the repository, set `AVP_TEST_PROJECT` to the repository root first.

The checks cover neutral axes, movement/look direction and sensitivity, input
focus and menu suppression, resumed movement, button actions/releases, and the
pause-menu hook. These exercise `ReadPlayerGameInput`, not a copied input formula.
Each of the eight stick directions is checked separately for its signed increment,
matching request flag, idle unrelated axes, analog mode, and release. Profile
checks cover restored legacy axis roles, missing sensitivities, and preservation
of explicit sensitivity/inversion choices. Controller-to-JOYINFOEX mapping has
separate coverage in `tests/controller`. These fixtures stop at input requests;
actual movement and camera behavior are also checked in the live game.
The same fixture also runs Marine controller-preset and legacy-profile migration
checks from `test_marine_preset.c`, including preservation of customized bindings
and an idempotent save/reload round trip.

Status shortcut checks use an announcement spy: H and Xbox View/Back request one
Marine readout per press, simultaneous shortcuts combine, and custom bindings
retain priority across all 27 active slots in both binding tables. Menus, console
input, death, other species, demos, completed levels, and same-frame pause/console
transitions suppress the request. These tests verify input routing; status text
formatting and screen-reader output are tested separately.

Tracker shortcut checks use a second announcement spy: T and Xbox D-pad Down
pass the current world position and heading once per press, combine simultaneous
tracker shortcuts, and preserve custom bindings across all 27 active slots in
both tables. The same gameplay gates apply, and missing player dynamics safely
suppresses tracker-only requests. If status and tracker are requested together,
status wins and eligible tracker edges are consumed, preventing a queued second
interruption. Custom-bound edges remain intact. Existing status behavior also
remains valid without player dynamics.

`test_gameplay_input.exe trace` provides a deterministic diagnostic sample:
14 gameplay lines and 3 binding-table lines. It exercises the 500 ms analog
throttle, neutral releases including a short excursion inside the throttle
interval, immediate jump/crouch edges, menu/focus transitions, changed bindings,
and a species switch. Stable state does not produce additional lines. The trace
contains numeric input state only.

Generated headers and binaries are ignored. Historical baseline snapshots and
captured result logs belong outside this test directory.
