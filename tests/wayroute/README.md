# Authored waypoint routing checks

Run `run_tests.bat`. It compiles the real `acc_wayroute.c` against small
`AIMODULE` and `WAYPOINT_HEADER` fixtures built from the engine's actual
`module.h` and `bh_waypt.h` types. No game process or retail map is used.

The suite covers direct and multi-volume paths, world-origin conversion,
vertically stacked volumes, one-way/reversed and Alien-only links,
probe-approved touching volumes, cyclic graphs larger than 100 nodes, malformed
indices, endpoint coverage and unchanged NPC scratch/output state. These checks
validate the authored graph search; they do not establish that a returned
waypoint is clear in live collision geometry.
