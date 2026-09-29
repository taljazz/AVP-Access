# AVP Access

An accessibility fork of *Aliens versus Predator Classic 2000*, aimed at making the
game playable without sight.

It is built from the engine source rather than modding the shipped binary, which is
what makes deep accessibility possible: speech, audio cues and navigation aids can be
driven from real game state instead of guessed at from the outside.

## Copyright and licence

**The source code to Aliens Vs Predator is copyright © 1999–2000 Rebellion, who are its
creators and owners.** It was released publicly by Rebellion for non-commercial use.

> The source code to Aliens Vs Predator is copyright (c) 1999-2000 Rebellion and
> is provided as is with no warranty for its suitability for use. You may not
> use this source code in full or in part for commercial purposes. Any use must
> include a clearly visible credit to Rebellion as the creators and owners, and
> reiteration of this license.
>
> Any changes made after the fact are not copyright Rebellion and are provided
> as is with no warranty for its suitability for use. You still may not use
> this source code in full or in part for commercial purposes.

See [LICENSE](LICENSE). This licence applies to this fork in full.

**No game data is included or redistributed here.** You need your own copy of
*Aliens versus Predator Classic 2000* from [GOG](https://www.gog.com/en/game/aliens_versus_predator_classic_2000)
or [Steam](https://store.steampowered.com/app/3730/).

## Lineage

- **Rebellion Developments** — the original game and the released source.
- **[icculus.org/avp](https://icculus.org/avp/)** — the original Linux port.
- **[atsb/NakedAVP](https://github.com/atsb/NakedAVP)** — the modern SDL3 / OpenAL /
  OpenGL port this forks from. Its README is preserved as
  [README-NakedAVP.md](README-NakedAVP.md).

## What this fork adds

**Speech.** Screen-reader output through Tolk (NVDA, JAWS, with a SAPI fallback),
loaded dynamically so a missing `Tolk.dll` degrades to silence rather than failing to
start.

**Spoken menus.** Narration captures rendered labels and values. The Single Player
graphics use localized species names; profile selection reads real profile names.
Menu titles are announced once on entry. See the handover for tested paths and limits.

**Spoken game text.** Messages passing through the on-screen-message hook are spoken.
Mission preludes are read on the mission briefing screens. This does not establish
speech coverage for every subtitle or text-rendering path in the game.

**Objectives and message history.** During Marine or Predator gameplay, **O / D-pad Right**
cycles visible objectives. Levels without stored descriptions say "No description
recorded." **F1 / right-stick click (R3)** replays stored mission messages; repeat
within four seconds to step backward. R3 is part of both default controller presets;
exact legacy presets gain the binding on load and custom bindings are preserved. Empty
history says "No mission messages yet."

**Xbox stick response.** Left stick moves forward/back and sidesteps; right stick
turns and looks. Gameplay sticks now ramp smoothly from their dead zone, with a
gentle response near centre and half the previous maximum movement/look input.
Existing profile sensitivity/inversion choices still apply. Menu navigation and
trigger/button mappings are unchanged. RT fires primary; LT fires secondary in
the default Marine preset (LT is not a modern aim-down-sights control).

**Route guidance (initial version).** Marine support is described below; Predator
route guidance shares the navigation system and has a scoped Waterfall opening
helper for the two surveyed jumps and gate02 approach. Both guided jump-assist
crossings have now been replayed in a disposable profile with the rebuilt binary;
later mission coverage remains in progress. Press **N**, or hold **View/Back**
and press **D-pad Right**, to toggle continuous guidance. A directional tone plays
once a second; speech names the next opening with a clock direction and distance.
Directions update as you move. Room/detour changes are spaced at least three
seconds apart; other changed readouts wait six seconds. Jump and actual
interaction readiness remain immediate. Guidance queues
speech rather than interrupting mission messages. Pausing, dying or restarting
turns guidance off; use the shortcut again to restart it. Custom bindings win.

This uses direct objective switches/areas and the engine's room connections.
When a whole route cannot be verified, it guides the reachable section toward a
closed door or an unverified passage and names that boundary. For supported doors,
it finds reachable direct controls and checks alternate paths past lock-only doors.
It announces when a control can actually be operated: press **X** on the default
Xbox layout. Breakable, non-explosive scenery blocking the control is announced;
aim at the obstruction and use **RT**, then **X** when prompted. Unknown
obstructions are identified without guessing how to remove them. It does not yet
solve every multi-stage door-control chain or arbitrary jump route; use sonar
alongside it. The opening Marine objective spans the level, so this is not a
complete campaign walkthrough. Unknown targets are explicitly reported.

Nearby route openings can now prompt **"Strafe left"** or **"Strafe right"**:
move the left stick sideways while keeping your facing. Local probes check body
clearance and supporting ground. For a low obstacle with clear overhead space
and nearby landing ground, guidance can say **"Jump forward a little"**:
use **A** and a brief forward movement on the default Xbox layout.
These sampled jump checks do not cover gap jumps or high ledges. They do not
simulate your jump trajectory; a long running jump can overshoot the checked area.

Local obstacle steering checks body clearance and floor support before choosing
an intermediate approach. Speech and the directional tone point to that same
waypoint, which stays stable as you turn and move. Detours start only when the
next 1.2 metres (plus body clearance) are obstructed. A cached detour releases
when the original three-metre approach clears. If no checked approach or
supported jump is found, it keeps the destination bearing and adds **"Check
path."** The tone remains an orientation aid, not a promise of clear walking
space. Uncertain scans cannot interrupt speech or repeatedly silence the beacon.
Use sonar to examine the nearby space. Strafe/jump instructions stand alone.
This is a limited local search;
stairs, uneven floors and longer detours can still require further refinement.

Complex rooms also use authored navigation volumes. Standing-height checks choose
an alternate point when an AI volume centre lies inside scenery. Intermediate
targets are retained until approached to reduce direction reversals. A bounded
grid search handles some detours requiring multiple corners.

Supported automatic lifts supply a route between floors: **"Lift. Wait."**
means stay aboard; **"Exit lift"** announces the departure direction. With guidance
on, the selected lift is held at the required landing while you are aboard, giving
time to hear the instruction. Stepping off or switching guidance off releases it.
This assistance is single-player only and does not apply to every kind of elevator.
The two long Derelict shafts are recognized across room boundaries. The final
lift waits through the short step-off area before returning. The entire map is
inventoried, and a survey from slot 2 has reached the level exit. Optional areas
and all puzzle states are not exhaustively live-verified. Completion now speaks
"Level complete." See the handover for evidence and survey-mode limitations.
**Automatic targeting guidance.** With continuous guidance on (**N** or
**View + D-pad Right**), a supported hostile within 20 metres and a clear line
of sight temporarily takes priority over route directions. Listen for
"Targeting", followed by turn and aim corrections. Use the **right stick** to
aim. A faster, higher cue and **"On target"** mean the current weapon ray reaches
that enemy; **RT** remains your primary-fire control. Guidance does not turn or
fire automatically, and weapon spread, recoil and projectile travel still apply.
It retains a visible target to avoid switching between nearby enemies. When no
target remains visible for 1.2 seconds, it resumes route guidance. This does not
mean the room is clear of enemies. Pausing or toggling guidance off cancels both.

Supported targets include living aliens, facehuggers, predators, predaliens,
queens and xenoborgs. Marine/Seal characters are included only when their AI is
targeting the player, so ordinary friendly Marines are excluded. Predator players
can identify armed Marine/Seal soldiers and hostile androids without waiting for
their AI to target the cloaked player; unarmed scientists and civilians are not
treated as armed threats. Living NPC Predators remain hostile to Marines regardless
of their current target, while a Predator player targets an NPC Predator only when
that NPC targets the player. This is single-player guidance; unknown entity types
are not targeted. Combat speech interrupts older announcements to avoid stale aim
instructions; mission messages remain reviewable through **R3/F1**.

**Status.** During gameplay, press **H** or the Xbox **View/Back** button
(the small button with two overlapping squares) to hear species-specific status.
Marine status includes health, armor, weapon, loaded ammunition, and spare
magazines; Predator status includes health, field charge, cloak, vision mode,
selected weapon, and relevant charge/ammunition. View announces on release only
when used alone; holding it for controller combinations stays silent. Keyboard H
remains immediate. Marine fuel, grenade types, and each pistol in a dual-pistol
loadout are described separately. Each press speaks once and interrupts older
speech. If either shortcut is already assigned to a gameplay action, that assignment
takes priority; the other shortcut remains usable.

**Supply guidance.** Press **L**, or hold Xbox **View/Back** and press
**D-pad Left**, to browse useful supplies: needed medkits first, then other
useful supplies by distance. Press **K**, or hold **View/Back** and press
**D-pad Down**, to start guidance to the selected supply, or to cancel it.
Collection and cancellation both return to the prior guidance state. Medkits and
armor are used by touching them; ammunition goes into inventory, weapons are
selected with **Y/LB**, and **RT** fires the current weapon. For Predator play,
guidance surfaces useful Predator weapons/ammunition and field-charge pickups when
charge is below its maximum. Marine medkits, armor and irrelevant ammunition are
not presented as Predator supplies. Collection follows the engine's pickup rules;
the helper does not edit inventory. This support is scoped to single-player.
Custom gameplay bindings take priority over these shortcuts.

**Snap to guidance direction.** With supported route or supply guidance enabled,
hold **View/Back** and click the **right stick** to face the current guidance
point. Visible combat targets take priority and include vertical aim; nearby
door controls also include vertical aim. Navigation turns horizontally and
levels your view. Each press snaps once, without moving or firing. A lost target,
unknown route, or waiting lift will not snap. R3 alone repeats messages for both
Marine and Predator;
L3 still walks. Custom gameplay assignments take priority. Sonar sweeps and
motion-tracker readouts do not select a navigation destination for this feature.

**Marine motion tracker.** Contact beeps now come from the detected direction,
with the game's existing distance tones and sweep timing. Press **T** or Xbox
**D-pad Down** to hear the nearest tracked contact ahead as a clock bearing and
approximate distance: 12 o'clock is ahead, 3 is right, and 9 is left. This uses
the game's current tracker blips and detection rules within its 30-meter range;
it does not detect every enemy. The tracker is unavailable while the image
intensifier is active. Custom gameplay bindings take priority over each shortcut.
If status and tracker are requested together, status is spoken first and a fresh
press is needed for the tracker.

**Sonar sweep.** Press **R**, or **D-pad Left**, to hear the shape of the space you
are standing in. Nine rays scan 180 degrees ahead; the game names what it finds --
"Corridor ahead. Walls 2 metres left, 2 metres right", "Dead end. Wall 1 metre
ahead", "Wall ahead, opening left" -- and pings each sector left to right. Walls use
the tracker's three distance pitches; openings use its click, so a way through never
sounds like a distant wall.

**Music and cutscenes.** Neither was implemented in the port. Both are restored by
decoding the Bink and Smacker files the retail release ships, via FFmpeg: the
soundtrack, the fullscreen intros and outros with picture and sound, and the in-game
wall-monitor briefings. Normal startup plays the original Fox Interactive and
Rebellion logo movie, followed by the title sequence and the dedicated
`fmvs/introsound.smk` menu theme. `--skip-intro` goes straight to the menus.

**Gamepad support.** SDL3's mapped gamepad layer provides Xbox menu navigation
and twin-stick movement/look. Controllers can connect or reconnect after startup.
A selects, B goes back, and Start opens the pause menu. Select Resume Game with A
to resume. Menu aliases are disabled while capturing a binding.

Marine has a default controller layout in the secondary bindings:

| Control | Marine action |
| --- | --- |
| Left stick up / down | Move forward / backward |
| Left stick left / right | Strafe left / right |
| Right stick left / right | Look left / right |
| Right stick up / down | Look up / down (with normal vertical setting) |
| RT / LT | Primary / secondary fire |
| A / B / X | Jump / crouch / interact |
| Y / LB | Next / previous weapon |
| RB | Throw flare |
| D-pad Up | Image intensifier |
| Hold left stick click | Walk (running is the default) |
| View/Back or keyboard H | Speak Marine status (when unbound) |
| D-pad Down or keyboard T | Speak nearest tracker contact (when unbound) |
| View/Back + D-pad Left or keyboard L | Browse useful Marine supplies (when unbound) |
| View/Back + D-pad Down or keyboard K | Guide to selected supply, or cancel supply guidance (when unbound) |
| Hold View/Back, click right stick | Snap to current route, supply, control or combat target |

Unchanged older Marine binding sets receive this layout when their profile loads.
Predator now has a default controller layout too; an exact legacy Predator layout is
migrated, while custom bindings are preserved. Alien action buttons still need manual
bindings. Primary keyboard/mouse bindings remain available. The Predator keyboard
defaults vary with the selected language layout, so use the in-game Controls screen
for movement and weapon keys.

Predator controller and accessibility controls:

| Control | Predator action |
| --- | --- |
| Left stick / right stick | Move and sidestep / look and turn |
| RT / LT | Fire primary / secondary weapon |
| A / X | Jump / operate |
| Y / LB | Next / previous weapon |
| D-pad Up / RB | Cloak / cycle vision mode |
| Hold View/Back + RB / LB | Zoom in / out; speech reports the zoom step (1–3) or normal view |
| Hold View/Back + X | Request disc recall; feedback confirms the request, not whether a disc was available |
| Hold View/Back + B | Select the Predator medicomp when possessed; RT heals and LT extinguishes fire |
| Hold View/Back + Y | Request the grappling hook when enabled by the current equipment |
| Hold View/Back + L3 | Taunt |
| R3 alone / keyboard F1 | Replay stored mission messages |
| Hold View/Back, click R3 | Snap to the current supported route or control target; this chord takes priority over R3 history |
| View/Back alone, then release / keyboard H | Speak Predator status |
| D-pad Left / keyboard R | Sonar sweep |
| D-pad Right / keyboard O | Cycle mission objectives |
| Hold View/Back + D-pad Right / keyboard N | Toggle route guidance |
| Keyboard J (when unbound) or hold View/Back + A | Start/cancel optional assist at a surveyed staging point when route guidance is on and the opening gate is unlocked; it runs and jumps using normal game physics |
| Keyboard T | Explains that the Marine motion tracker is unavailable to Predator |

Accessibility shortcuts respect custom bindings; a conflicting configured action keeps
its binding. The default Predator preset assigns R3 to history. Exact legacy/default
presets gain that assignment on load; customized presets are left untouched. The
saved Predator profile also receives its gamepad secondary preset when its active
secondary slots exactly match the old keyboard-only default and its primary contains
no gamepad bindings. This preserves the custom keyboard primary and unused expansion
bytes; customized secondary actions or gamepad primary bindings block the migration.
View+A assist chord is consumed as a deliberate request only when View is unbound and
A retains its default Predator Jump binding; ordinary solo A still jumps normally. Starting also requires active route guidance and the unlocked opening gate.
The assist cancels on manual movement/look/crouch/fire, route disable, focus loss or
scope loss, and never writes position or velocity. It has been physically replayed
through both surveyed Waterfall jumps; manual traversal between them, normal-combat
behavior and screen-reader/controller listening remain unverified. The latest build
also completed a targeted guided replay from the saved pre-lift checkpoint through
the final shaft and airlocks. Survey mode does not establish normal-combat survival.
This list documents implemented input behavior, not user-side controller or
screen-reader listening.

The new View equipment chords borrow only the exact stock Predator bindings and are
disabled when View or the chord button has a custom binding. They consume the matching
standalone action while held; tap the button again after releasing it to repeat while
keeping View down. Vision changes are spoken by mode name. Zoom reports its numbered
step, without claiming a magnification. Grapple remains unavailable unless the game
has enabled the equipment, and disc recall speech confirms only that a request was sent.
The plasma caster's engine lock feedback has no new spoken accessibility cue in this change.
These new equipment chords and vision announcements have automated coverage but have
not yet had a live controller or screen-reader listening check.

Several long-standing bugs in the engine were fixed along the way — see
[docs/HANDOVER.md](docs/HANDOVER.md), which documents the engine's traps in detail.

## Building (Windows)

Visual Studio with the C++ workload is the only thing you need to install — it ships
CMake and Ninja, and `tools\` finds them for you.

Expected layout, with this repository checked out as `NakedAVP`:

```
AVP Access\
  NakedAVP\      this repository
  third_party\   SDL3-*, openal-soft-*, ffmpeg-*  (unpack them, names are globbed)
  game\          your game data, copied and lowercased
  build\         created for you
```

Then:

```
NakedAVP\tools\build.bat     configure if needed, build, stage the runtime DLLs
NakedAVP\tools\run.bat -w    run windowed (arguments are passed through)
```

`tools\env.bat` locates Visual Studio with `vswhere` and finds the SDKs by globbing
their version-stamped folder names, so nothing is pinned to one machine; every value
can be overridden from the environment (`VSROOT`, `SDL3_DIR`, `OPENAL_DIR`,
`FFMPEG_ROOT`, `AVP_ROOT`, `TOLK_DIR`).

FFmpeg is optional — without it the game builds and runs, simply with no music or
cutscenes.

`tools\stagedlls.bat` copies the runtime DLLs next to the executable. One step there is
easy to miss by hand: **OpenAL Soft ships as `soft_oal.dll` and must be renamed to
`OpenAL32.dll`**. Tolk is not vendored — it is a separate project with its own licence —
so point `TOLK_DIR` at a folder containing `Tolk.dll`, `nvdaControllerClient64.dll` and
`SAAPI64.dll`, or drop them into `build\` yourself. Without it the game runs, silently.

Game data must be lowercase, files and folders both; `lower.sh` does that on a
case-sensitive filesystem.

Linux and macOS should still build — the accessibility layer is guarded so platforms
without Tolk or FFmpeg compile to no-ops. The `tools\` scripts are Windows-only;
elsewhere invoke CMake directly.

## Diagnostics

The [whole-level inspection tool](docs/MAP-INSPECTION.md) exports room connections,
base geometry, switches and door states through the bridge's `map` command.
`tools\inspect-map.ps1` produces a text report and can trace switch request chains.
`tools\audit-map.ps1` checks graph references and reports authored volume and lift
coverage. Disposable survey-mode play physically reached `Level complete` on
Waterfall at trace4 sequence 1834. The completion screen is
`../build/predator-jump-assist-trace4-20260920/bridge/shot-001836.png`, with speech
in that session's `bridge/events.jsonl`. A later targeted replay from the saved
pre-lift slot8 used the latest executable built with `tools\build.bat` through
the lower switch, lift ride, staged exit, upper switch, airlocks and
`Level complete` (event 392, frame 1220;
`../build/predator-final-guided-replay2-20260920/bridge-clean/events.jsonl`).
This verifies that segment, not a fresh guided campaign. Both runs used survey
mode and do not establish normal-combat survival. The source has a scoped opening
route and measured final-lift itinerary; route (159 checks) and lift-route (63
checks) regressions pass. The earlier jump replay clone profile is
`../build/predator-jump-assist-replay-20260920/profile/.avp`; the final-route
replay profile is `../build/predator-final-guided-replay2-20260920/profile/profile/.avp`.
The original stage5 gate05 profile remains separate and preserved. Do not infer
general Predator accessibility, normal-combat success, NVDA speech quality or
controller feel. Alien movement still needs its own rules
and live validation; see [the handover](docs/HANDOVER.md).

The optional [local play bridge](docs/BRIDGE.md) provides screenshots, bounded
input commands and timestamped speech/cue logs for development. It is off during
normal play; `tools\bridge.ps1` is its PowerShell client.

Flags added for testing without having to play to the content in question:

| Flag | Purpose |
| --- | --- |
| `--movie <file>` | Play one FMV and exit |
| `--plotmsg <n>` | Decode one wall-monitor briefing and report on it |
| `--padtest [secs]` | Report what SDL sees from the controller |
| `--trackertest` | Guided speech and directional tracker listening examples |
| `--padtrace` | Trace how pad state reaches the engine |
| `--intro` / `-intro` | Play the startup logo sequence (default) |
| `--skip-intro` | Skip the startup logos and title sequence |
| `-w` / `-f` | Windowed / fullscreen |

Note that MSVC has no `getopt_long`, so the Windows build previously ignored *every*
command-line option; a hand-rolled parser was added.

## Guided tracker listening test

Run `tools\run.bat -w --trackertest` for six clearly announced simulated contacts
using the game's actual tracker samples, speech and spatial audio. No level or
profile menu is entered. Headphones make left/right comparisons easier.

Press **A**, **Enter** or **Space** for each example. After the spoken bearing and
distance, wait for two beeps. **D-pad Down** or **T** plays the current example's
beeps immediately, without repeating the speech. **B** or **Escape** ends the test. Pressing next cancels any pending beeps from the
previous example. Advancing once more after the sixth example closes the test.

The sequence is left at 12 meters, ahead at 12 meters, right at 12 meters, ahead
at 5 meters, ahead at 25 meters, then the same 12-meter world contact with the
listener turned right (so it is now on the left). Near/far examples use the game's
corresponding distance tones. These examples test listening and speech; detecting
actual gameplay contacts remains a separate check.

## Regression checks

Each suite compiles the *real* module against mocked engine data, so they test the
shipping code rather than a copy of it. None needs a controller, a game window or the
retail data. They need only MSVC and PowerShell — run `tools\env.bat` first, or let the
runners do it.

`tests\run_all.bat` runs every runner below, prints pass or fail for each, keeps each
runner's full output in `%TEMP%\avp-access-tests` (or a folder given as its argument),
and exits non-zero if any fail.

| Suite | Covers | Checks |
| --- | --- | ---: |
| `tests\controller\run_tests.bat` | Menu press/hold/release, any-key prompts, overlapping keyboard input, binding capture, disconnect/reconnect, sticks, triggers, menu-to-gameplay transitions | 240 |
| `tests\tracker\run_tests.bat` | Contact descriptions, bearing and distance boundaries, speech, spatial-audio parameters and bridge cue scope | 225 |
| `tests\tracker\run_listening_tests.bat` | Guided listening diagnostic: examples, timing, controls, cancellation, unavailable audio, playback failure, state cleanup | 355 |
| `tests\status\run_tests.bat` | Spoken Marine and Predator status against engine data, ammunition/charge details, invalid state, speech calls and Predator vision modes | 98 |
| `tests\gameplay\run_input_tests.bat` | Gameplay input, menu and focus blocking, stick directions, Marine/Predator preset migration, R3 history, custom bindings, equipment chords, bridge clock, route toggle, supply controls, snap chord, optional jump-assist chord and cancellation | see runner output |
| `tests\tracker\run_hud_tests.bat` | Actual HUD detection, sweep timing, contact snapshots, independent sonar/objective resets, gameplay eligibility and scan-click logging | 49 |
| `tests\sonar\run_tests.bat` | Space shape naming, sector openings, distance rounding, ping schedule and reset, with the raycast mocked to build synthetic rooms | 37 |
| `tests\media\run_tests.bat` | Menu music selection and restart behaviour, media fallback when FFmpeg or a file is missing | 22 |
| `tests\objectives\run_tests.bat` | Objective wording, missing descriptions, cycling and reset | 18 |
| `tests\bridge\run_tests.bat` | Commands, bounded input runner, turns, JSON, PNG encoding and independent image decoding | 168 |
| `tests\bridge\run_runtime_tests.bat` | Actual filesystem failures/locks, retained replies/events, held-close cleanup, PowerShell client sequencing, timeout handling map export and survey opt-in | 61 |
| `tests\history\run_tests.bat` | Empty and populated history, browsing/expiry, bounded formatting and save-block validation | 21 |
| `tests\menu\run_tests.bat` | Species labels, capture boundaries, menu transitions, duplicate suppression, profiles and briefing assembly | 13 |
| `tests\route\run_tests.bat` | Room graph, alternate door paths, movement/interaction priority, combat takeover/resume, snap targets, surveyed shafts, Predator opening/final-lift integration, lower stair approach, run-up timing and room-9 handoff | 159 |
| `tests\fall_route\run_tests.bat` | Scoped Waterfall opening phases, position reconciliation, measured run-up speed/alignment, jump/landing corridors, recovery and gate approach | 1627 |
| `tests\snap\run_tests.bat` | One-shot yaw/pitch, limits, state synchronization, gameplay gates and preservation of movement/fire | 20 |
| `tests\loot\run_tests.bat` | Marine and Predator supply usefulness, species-specific ammo/weapon filtering, field charge, selection lifecycle, collection confirmation, proximity and LOS behavior | 50 |
| `tests\route\run_interaction_tests.bat` | Actual interaction selection, range/alignment, occlusion, competing controls, activation and breakable scenery classification | 16 |
| `tests\traversal\run_traversal_tests.bat` | Analytic floor/box geometry, species-specific standing clearance, lateral clearance, low obstacles, ceilings, gaps, local detours, waypoint cache and shared LOS preservation and slopes | 53 |
| `tests\combat\run_combat_tests.bat` | Marine and Predator hostility rules, death, range, visibility, target retention, aim directions, actual ray alignment, loss/resume, snap validity and cue timing | 55 |
| `tests\route_targets\run_tests.bat` | Direct mission/door activators, world-space areas, request flags, prerequisites and bounded enumeration | 69 |
| `tests\wayroute\run_tests.bat` | Authored graph, species/one-way restrictions, vertical separation, checked supplemental links and alternate standing points | 25 |
| `tests\lift_route\run_tests.bat` | Platform selection, boarding, arrival, floor-contact discrimination and landing-hold release, including the surveyed Fall Predator shaft exit | 63 |
| `tests\jump_assist\run_tests.bat` | Scoped start/run-up, predictive launch, flight/landing, recovery and first-jump roof-contact tolerance | 337 |
Counts mix scenario assertions and parameter sweeps; consult each runner's output
for its exact result. The review and remaining validation gaps are in the handover.

**What they do not cover.** These fixtures inspect engine call parameters, not perceived
audio: they can confirm a cue was requested at the right position and volume, never that
it *sounds* like it is to your left. Directional audio, speech intelligibility and
controller feel all need a live test, and the handover records which of those a person has
actually confirmed.
