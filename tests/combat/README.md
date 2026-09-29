# Combat guidance unit checks

Run `run_combat_tests.bat` from a Visual Studio developer shell. It compiles the
real `src/access/acc_combat.c` against small engine mocks; no game process, map,
audio device, or GPU is required.

The runner covers hostile species and life-state exclusions, Marine/Seal player
target confirmation and civilian rejection, Android labeling, health/range/LOS
gates, acquisition persistence and scan throttling, gun-ray-only alignment,
lost-sight grace/reacquisition, aligned cue cadence, silent reset, and restoring
the engine's shared LOS result globals.

The LOS mock intersects small actor-centered spheres in memory and supports an
explicit occlusion flag; it does not model walls, modules, or the game collision
mesh. Camera vector rotation follows the engine's fixed-point matrix operation,
with identity and one quarter-turn tested. Actual cue audio, live facing state,
and full route takeover remain outside this unit runner. The implementation
currently uses a 450 ms non-aligned cue interval; this runner asserts the 200 ms
aligned cadence. Main integration and live-game validation remain separate.
