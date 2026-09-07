# Spoken Marine status checks

Run `run_tests.bat` from a Windows command prompt. The runner uses `tools/env.bat`
to locate Visual Studio and the engine headers. It compiles the actual
`src/access/acc_status.c` with mocked engine data and speech output; it never
opens the game or speech backend.

Set `AVP_TEST_PROJECT` to the repository root when running a staged test copy.
An optional first argument selects another `acc_status.c`; its matching header
must be beside it. Exit status is 0 for success, 1 for failed assertions, and 2
for setup/compilation failure.

The 89 assertions in 12 scenarios cover difficulty-specific percentages and
rounding, negative/overfull values, wide arithmetic, primary and secondary ammo,
dual pistols, selected grenade types and live counts, fuel, unavailable weapons,
invalid names and indices, buffer boundaries, and immediate/repeated speech.
The gameplay fixture in `tests/gameplay` separately verifies the H/View shortcuts,
press-edge handling, custom binding priority, and menu/focus suppression.

Hearing the spoken status through the user's screen reader remains a live test.
