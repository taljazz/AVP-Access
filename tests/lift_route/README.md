# Platform-lift route checks

Run `run_tests.bat`. It compiles the real `acc_lift_route.c` and builds small
engine-structure fixtures for the player, platform lift, active-object list,
AI rooms, and collision reports. No game process or retail map is used.

The suite checks lift eligibility and room scope, boarding/wait/exit states,
platform floor-contact discrimination, and rejection of targets too far from
either lift terminal. It verifies routing decisions only; live platform timing,
boarding geometry and retail-level access remain separate checks.

Derelict fixtures also cover the two identified shafts, cross-room platform
contact, the final shaft's standing-height offset, and a bounded lower-landing
hold during step-off contact gaps. Other levels/platforms retain the original
contact requirement. Reset and leaving the step-off area release the hold.

Fall/Predator fixtures cover exact unique platform identification across the
surveyed shaft rooms (including the disabled one-use platform), the measured
2913mm standing offset, the east-side lower boarding point, and the staged
north-then-west upper exit. Completion requires grounded contact at the measured
upper-floor handoff; ambiguous lifts, unrelated rooms/species, premature floor
positions, and falls outside the exit region are rejected. The live survey
verified those positions and the final mission trigger; route integration and
normal-combat play remain separate validations.
