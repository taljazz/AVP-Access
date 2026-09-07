# Sonar tests

Compiles the real `src/access/acc_sonar.c` and mocks the engine's line-of-sight
routine, so a synthetic room can be described ray by ray -- a corridor, a dead
end, a doorway -- without a game, a level, an OpenAL context or a speech backend.

The clock is passed into `AccSonar_Request()` and `AccSonar_Update()` rather than
read inside the module, which is what makes the ping schedule deterministic here.

Run `run_tests.bat`. Thirteen scenarios, 37 assertions:

| Case | Checks |
| --- | --- |
| `bearings` | Ray fan spans 180 degrees, middle ray dead ahead, 22.5 degrees apart |
| `open` | Nothing struck reads as open space, with no wall distances |
| `corridor` | Walls both sides and open ahead is named a corridor |
| `dead_end` | No opening in any sector is a dead end |
| `doorway` | A gap beside a wall is named as an opening |
| `side` | An open side off a corridor is named |
| `wall_ahead` | Nearest hit in a sector is the one reported; singular wording |
| `sub_metre` | A surface under a metre is not rounded to zero |
| `rounding` | Distances round to the nearest metre |
| `bands` | Near, mid and far walls select the high, middle and low tone |
| `click` | Openings ping as a click, walls as a pitched tone |
| `schedule` | Pings become due left, ahead, right at 500 ms, and never twice |
| `reset` | Reset cancels pending pings and stops a sounding cue |
| `null` | A missing player casts no rays and says nothing |

These inspect engine call parameters, not perceived audio: they confirm a ping
was requested at the right position with the right sample, never that it sounds
like it is to your left. That needs a person.
