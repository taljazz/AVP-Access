# Local movement geometry checks

Run `run_traversal_tests.bat`. It compiles the real `acc_traversal.c` and intersects
its rays with analytic box faces representing floors, walls, low obstacles,
ceilings and landing gaps. No game data, audio, GPU or Python is needed.

The checks cover relative-yaw lateral directions, blocked sideways movement,
floor support, a positive low-obstacle jump candidate, rejected tall walls,
ceiling/landing failures, grounded/standing/gravity/encumbrance gates and
restoring the engine's shared raycast results. Steering fixtures cover pillar
detours, genuinely impassable grid-spanning walls, a too-narrow opening, floor
gaps, wall-touch movement, wall-mounted control stand-off, multi-corner cached
path progress, changed goals, new obstacles, explicit reset and recovery after
clearance. Route integration/timing is checked separately by
`tests/route/run_tests.bat`.

These tests validate sampled geometry and local steering, not an actual Marine
jump trajectory or complete map route. Retail-map traversal and user listening
still need live verification. A long running jump can overshoot the four-metre
sampled region.
