# Waterfall opening fall-route checks

Run `run_tests.bat`. It compiles the pure `acc_fall_route.c` phase helper with
MSVC and synthetic caller state; it does not require the game or a retail map.

The cases cover fail-closed Predator/level/room/gate scope, upper-floor gating,
position reconciliation, grounded run-up speed and heading criteria, early jump
cue windows, missed-takeoff recovery, airborne landing guidance, landing-zone and
floor-height checks, north-deck waypoints, and the final gate-approach leg. The
helper only returns guidance data; it does not move or jump the player. These
checks do not certify a live route beyond the measured points recorded in the
handover.
