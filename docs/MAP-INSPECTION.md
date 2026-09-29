# Whole-level inspection

This development tool exports the loaded level, not just the visible rooms.
It does not change navigation, move the player, unlock doors or advance a held
bridge simulation. The exporter works from shared engine structures rather than
Marine-only geometry. A local Waterfall Predator snapshot has been inspected;
that does not validate arbitrary Predator maps or Alien live exports.

In a running bridge gameplay session:

```powershell
.\tools\bridge.ps1 -Directory ..\build\map-session -Command map
.\tools\inspect-map.ps1 -Path ..\build\map-session\map.json -Output ..\build\map-session\report.txt
.\tools\audit-map.ps1 -Path ..\build\map-session\map.json -Output ..\build\map-session\coverage.txt
```

Add `-ObjectId 46` to the report command to trace incoming switch requests for
object 46 in that particular snapshot. IDs are array indices, not persistent
identities. Never reuse them after loading or exporting another session.
Cycles in the request graph are visited only once. Requests are reported as raw
integers: their meanings depend on the receiving behavior. A link is not proof
that operating the sender will complete a puzzle.

## Snapshot schema 1

- `level`, `species`, `player`: level name, engine species enum and player XYZ.
- Coordinates are millimetres, with positive Y downward. Euler units are 4096
  per turn. Mesh vertices are **local**, not world coordinates.
- `rooms`: AI room graph, world position, current AI passability, directed links,
  world-space entry points, and Alien-only restrictions. Missing entry data is
  explicit. AI passability is not a species-specific player navigation guarantee.
- `waypoints` within each room: authored volumes with room-local centres, bounds
  relative to those centres, flags and directed links. Reverse-oneway and
  Alien-only restrictions are preserved. NPC volumes are not a player navmesh.
- `modules`: render modules within each room, IDs/names, local bounds, world
  position, flags, associated object ID, base mesh position/Euler transform,
  vertices and indexed triangle/quad faces with polygon flags.
- `objects`: active strategy objects including objects outside camera visibility;
  snapshot IDs, type codes, module, position and rotation when present. Static
  objects, scenery and switches also carry base meshes where available.
- Binary/link switches include states, modes, security, trigger volumes, target
  request edges and link-switch prerequisites. Doors include supported current
  states/locks. Other behavior types retain their numeric engine type.
- Platform lifts include enabled/one-use flags, current state and upper/lower Y.

Base meshes do **not** resolve current morph animations, destroyed mesh fragments,
skeletal poses or final collision. Missing meshes are null, skipped non-polygon
items are counted. Terrain classification, clearance and species-specific
walkable surfaces must be derived and verified separately. There is no automatic
wall-crawling or jumping route planner in this tool.

`map.json.tmp` is closed and renamed only on success; a failed publication leaves
the previous map intact. The command is rejected outside gameplay. Exports
contain proprietary retail geometry: keep them and derived reports in the local
build directory. Do not commit or distribute them.

## First Marine inspection

The Derelict test checkpoint exported 172 AI room entries, 187 render modules and
388 active objects. Module base geometry contains 21,994 vertices and 14,674 faces;
including exported object meshes, 21,164 face index lists were validated.
Two consecutive exports were identical without advancing the frame or clock.

AI room 123 combines COMM-01, egg12 and COMM-03; their vertical extents differ.
The physical switch at `(29607, -298, -58748)` belongs to COMM-01 and sends eight
requests, including both zero and one values. Subsequent live inspection found
the automatic platform connecting its upper walkway to the lower switch floor.
The route now goes around the walkway to that lift, waits while aboard and
announces the exit at the destination landing. An isolated bridge replay reached
and operated the override, confirming its state changed and the lift was disabled.

The static audit covers 337 directed room links (12 Alien-only), all with entry
points, and 127 authored volumes across seven complex rooms. No invalid graph
references were found. Disconnected walking-volume pairs include restricted
crawling/ceiling routes; they do not prove the playable level is disconnected.
Seven platform lifts are exported. Simple rooms continue using portal guidance.

## Runtime routing limits

The 2026-09-20 Derelict survey reached the level-completion screen from a copy of
SkyPulse slot 2. The long ship shaft (75->76->158) and final shaft (171->159) are
platform descents, despite their NPC vertical restrictions. Exact directed
exceptions now route via the real lower landings, with boarding/wait/exit cues;
they do not waive restrictions elsewhere. A local per-room audit covers all 172
rooms: 162 have a topological path to END using those exceptions. This is not
live verification of all optional areas. Detailed evidence and limitations are
in `HANDOVER.md`; exported geometry and reports remain outside the repository.

Marine guidance uses authored volume links without changing NPC scratch state,
respects direction/species restrictions and can supplement touching volumes only
after a live floor/body-clearance check. If a centre is occupied at standing
height, it samples inset positions within that volume. This fixed the lower
COMM-01 centre that led into scenery. Selected intermediate destinations are
retained until approached, rather than reversing at overlapping volume edges.

Local steering first tries its short two-leg search, then a bounded 31-by-31 grid
at 800 mm spacing with at most 400 expansions. Cached legs are rechecked against
live collision. Supporting floor samples are spaced at most 300 mm apart.
This is sampled, local standing-Marine navigation, not a full collision navmesh:
slopes, moving objects, narrow passages and arbitrary jump routes remain limits.
Failed searches retain the destination bearing with "Check path." No obstacle
search unlocks a door, presses a control or moves the player. The separate lift
assistance holds the selected single-player automatic platform at the requested
landing while the guided player remains aboard; stepping off or disabling guidance
releases it. Arrival is checked every 100 ms during a wait. Departure clearance
extends beyond the short ordinary walking lookahead.

The export/audit covers the entire loaded map. It does not certify every route,
all puzzle states, combat outcomes, or Predator/Alien movement. See HANDOVER.md
for the exact live replay coverage and outstanding validation.

## Waterfall Predator first-level snapshot

The 2026-09-20 local audit contains 112 AI rooms, 160 render modules and 227
directed links; 52 links are species restricted. It reported no missing entries
or invalid references. This static graph and the stage3 checkpoint identify the
first opening control near `(26790, 15167, 9780)`. Disposable surveys physically
verified that switch, two opening jumps, gate02 into tunnl01, the later gates,
the final shaft and airlock, and reached `Level complete` at trace4 sequence
1834 (`../build/predator-jump-assist-trace4-20260920/bridge/shot-001836.png`,
speech logged in that session's `bridge/events.jsonl`). This completion occurred
in survey mode and does not establish normal-combat survival. The graph still
has a directed edge from room 9 into room 94 and no return edge; no general
exception was added. Stage5's original profile remains preserved at gate05
(slot 7 in the security area, slot 8 inside gate05). The earlier jump-assist
clone profile is `../build/predator-jump-assist-replay-20260920/profile/.avp`;
its slot 7 is after the upper switch and slot 8 is the pre-lift well01
checkpoint. Trace4 itself contains bridge logs, not a profile. In that earlier
clone, J completed the first guided jump and View+A completed the second; both
landed grounded. The latest source includes the measured final-shaft matcher,
north-then-west exit and lower-stair switch approach; route 159 and lift-route
63 checks pass. A targeted route replay from the saved pre-lift slot8 then used
this built source to reach `Level complete` after the switch, lift ride, staged
exit, upper switch and airlocks. Its session log is
`../build/predator-final-guided-replay2-20260920/bridge-clean/events.jsonl`
(event 392, frame 1220); this was not a fresh campaign replay. The replay clone
profile is `../build/predator-final-guided-replay2-20260920/profile/profile/.avp`.
Keyboard J and View+A start the optional assist only at surveyed staging positions with route
guidance enabled and the opening gate unlocked; moving controls cancels it, and
solo A retains ordinary jump behavior. Survey mode suppresses combat takeover
and makes the player immortal; these replays do not establish normal-combat
survival or success. NVDA speech quality and controller feel remain unverified.
The snapshot and reports are local under
`../build/predator-access-firstlevel-stage3-20260920/` and
`../build/predator-downstream-stage3-20260920/`; do not commit or distribute the
retail-derived map data. See `HANDOVER.md` for evidence limits.
