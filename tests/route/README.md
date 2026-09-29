# Route guidance checks

Run `run_tests.bat`. It compiles the actual `acc_route.c` with engine boundary
mocks; no retail data, game window, speech output or audio device is needed.

Coverage: 130-module routes, cycles and one-way links, closed doors, Marine-only
traversal, near-side boundaries for partial routes, heading/distance formatting,
continuous cue timing, changed-room announcements, unresolved targets and reset.
It also covers reachable door controls, alternate topology paths past unusable
doors, breakable-obstruction wording and actual interaction readiness.
Movement hint integration checks prompt timing, no repeated unchanged instruction,
disabled guidance, and interaction/unresolved-door priority. Geometry itself is
tested separately in `tests/traversal` against analytic floor/box intersections.
Combat integration checks takeover before the one-second route tick, stopping the
route cue, suppressing route speech, immediate resumption and shared reset.
Local steering integration checks matching detour speech/beacon waypoints,
original-target cue retention with throttled uncertainty on failed searches, and steering
to a diagonally distant control in the same room.
`run_interaction_tests.bat` compiles the real `triggers.c` and checks selection,
occlusion, alignment/range boundaries, competing targets and activation. Its
obstruction checks distinguish non-explosive breakable scenery from indestructible
objects, explosive objects, pickups and world geometry.
The related `route_targets` suite checks real switch/mission matching. Gameplay
and tracker HUD suites cover shortcuts, custom bindings and lifecycle gates.

Survey regressions cover the VIEWD-05 approach and the three directed Derelict
shaft connections, rejecting changed coordinates, other levels and reverse
climbs. They verify a missing lift stays in wait state and that combat takeover
is suppressed only in explicit diagnostic survey mode.

These checks do not certify walkability inside a room, lifts/jumps, audible
direction perception, or completion of a campaign. Live bridge evidence and
remaining limits are recorded in `docs/HANDOVER.md`.
