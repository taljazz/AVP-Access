# Route target resolver tests

Run `run_tests.bat` from this directory. It compiles the real
`acc_route_targets.c` against live-list and objective/switch mocks; it does not
launch the game. The checks cover area-volume center coordinates, objective
matching, direct request flags, missing physical placement, nearest-target
selection, and link-switch requests. Objective visibility and achievement state
are intentionally outside the resolver API and remain the caller's responsibility.

Door checks cover live receiver types, request flags, eligibility, unchanged
outputs on failure and enumeration termination after the last candidate.
