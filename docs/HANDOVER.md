# AVP Access — engine notes and handover

Written so that someone picking this up cold — human or another assistant — starts with
the map rather than rediscovering it. The original engine notes were verified against
the running game. The controller follow-up below distinguishes automated checks from
live observations and work still awaiting confirmation.

The theme, if there is one: this engine is 25 years old and the Windows path of the port
is not continuously tested. Several bugs below had been dormant for years because nothing
happened to exercise them. **When something behaves oddly, measure before theorising** —
three of the four hardest problems in this project were misdiagnosed on first inspection.

---

## 1. Layout

```
APV Access/
  NakedAVP/      this repository — engine source + src/access/
    tools/       env.bat, configure.bat, build.bat, run.bat
    tests/       controller, gameplay, status and tracker regression checks
  game/          working copy of the retail data, lowercased (NOT in git)
  third_party/   SDL3, OpenAL Soft, FFmpeg (NOT in git)
  build/         build output + runtime DLLs
```

The retail install is left pristine; `game/` is a copy. Data files and folders must be
lowercase — the port expects it.

**Staging trap:** do not exclude `*.dat` when copying game data.
`fastfile/aliensound.dat`, `marsound.dat`, `predsound.dat` and `queensound.dat` are the
species sound banks, not installer junk. Likewise the `FMVs/` folder is not optional —
the *music* lives there, as Bink files.

## 2. The accessibility layer

All new code is in `src/access/`, kept separate from engine source:

| File | Role |
| --- | --- |
| `acc_speech.*` | Tolk screen-reader output |
| `acc_status.*` | On-demand Marine health, armor, weapon, and ammo speech |
| `acc_tracker.*` | Directional tracker cues and spoken contact bearings/distances |
| `acc_menu.*` | Spoken menus, via render-text capture |
| `acc_media.*` | Bink/Smacker music, cutscenes, plot messages (FFmpeg) |
| `acc_pad.*` | SDL3 gamepad |

Each degrades to no-ops when its dependency is absent, so other platforms still build.

## 3. Engine hook points

**Speech** — Tolk is loaded with `LoadLibrary`, not linked. Its exports are plain
`__cdecl`; its C++ `bool` is one byte, so `unsigned char` is ABI-correct in the function
pointer typedefs. Game text comes from `language.txt` in a legacy 8-bit encoding — convert
with **`CP_ACP`, not `CP_UTF8`**.

**Menus** — `AVP_MENUS AvPMenus` is *file-static* in `avp_menus.c`, so the hook must live
there. `AccMenu_Poll()` is called at the end of `AvP_UpdateMenus()`, after
`ActUponUsersInput()` — one site that covers both the front end and the in-game menus, and
placing it after input means the announcement matches the keypress.

Narration works by **capturing the text the renderer draws**, not by re-deriving it.
`RenderMenuElement` routes nearly all text through one function pointer, so substituting a
capturing wrapper is correct for all ~40 element types by construction. Re-deriving it was
tried first and was wrong: episode entries use `TextDescription + slider offset`, and
profile and save slots keep their text in `c.TextPtr`. Those were the silent menus.

Two element kinds still need special handling:
- `AVPMENU_ELEMENT_GOTOMENU_GFX` (the Single Player species entries) draws no text at all.
  Its `HelpString` is a real localised label — use that.
- `AVPMENU_ELEMENT_USERPROFILE` is a *single* element that paints its own scrolling list,
  so there is nothing to capture and no per-profile element to count. Read the name via
  `GetFirstUserProfile()`/`GetNextUserProfile()`, indexed by `c.SliderValuePtr`.

`GetTextString()` is bounds-checked at the top end but **not against negative IDs**, and can
return an empty slot. Funnel every lookup through a guard.

**Game text** — `NewOnScreenMessage()` in `hud.c` is the single choke point for *every*
in-game message: objectives, pickups, plot text, multiplayer chatter, ~179 call sites. One
hook speaks all of them. Queue rather than interrupt; several often arrive together.
Mission preludes are `BriefingTextString[5]`, file-static in `avp_menus.c`.

**Audio, for future work** — `Sound_Play(SOUNDINDEX, "dlev", ...)` (`avp/psnd.h`) plays
looping 3D sources with a live handle; `Sound_Update3d()` moves them and
`Sound_ChangePitch()` covers ±4 octaves. That is a complete audio-beacon toolkit, already
spatialised by OpenAL. `FindPolygonInLineOfSight()` (`avp/los.h`) is a geometry raycast —
the primitive for a sonar sweep. `MotionTrackerBlips[]` in `hud.c` already computes entity
positions relative to player facing. `m_link_ptrs` (`include/module.h`) plus the waypoint
network in `bh_waypt.h` form an AI navigation graph that could drive route guidance.

## 4. Media

Cutscene video is scaled with swscale straight into the game's 640×480 **RGB565** software
surface (`extern SDL_Surface *surface`) and shown with `FlipBuffers()`, which already
handles aspect fitting — far simpler than managing a GL texture. Cutscenes are 640×360, so
they letterbox at y=60.

Sync uses the **audio clock** (`samplesRetired + AL_SAMPLE_OFFSET`) as master, or speech
drifts out of lip-sync. `AL_SAMPLE_OFFSET` is per-buffer here; verified against wall time.

The in-game plot messages (`message<N>.smk`, triggered from `bh_mission.c` via
`StartTriggerPlotFMV`) are all 128×96 **pal8** at 15fps with mono audio — exactly the format
`NextFMVTextureFrame`/`UpdateFMVTexturePalette` already want, since Smacker decodes natively
to paletted. Copy `frame->data[0]` respecting linesize; the colour table is `frame->data[1]`
as 256 native-endian `0xAARRGGBB` words.

**The FFmpeg trap.** `avcodec_send_packet` returns `AVERROR(EAGAIN)` when the decoder still
holds undrained output. Ignoring that return and unref'ing the packet anyway **silently
discards it**. Exactly half the video frames vanished this way, because audio buffers ~2s
ahead and kept the video queue permanently full. Hold the refused packet and retry it, and
make the demux step report "no progress" so callers stop instead of spinning. The symptom
was subtle: playback looked and sounded fine, just at half frame rate.

Also: these cutscenes open on ~2 seconds of genuine black, so a "no pixels" check on frame 1
proves nothing. Sample around frame 120.

## 5. Bugs found in the engine

All pre-existing, all worth upstreaming.

1. **`files.c` double-free → heap corruption (`0xC0000374`).** `OpenGameDirectory` frees
   `localdirname` on failure *without nulling it*, then stores the dangling pointer and lets
   `CloseGameDirectory` free it again. `~/.avp/<dir>` never exists, so **any** directory scan
   hits it. `CloseGameDirectory` also never freed the handle itself. This lay dormant because
   nothing in the exercised code path had scanned a directory.

2. **`files.c` inverted glob match on Windows.** `PathMatchSpec(pattern, filename) == 0` —
   the real signature is `PathMatchSpecA(file, spec)` returning TRUE on a match, so the
   arguments were swapped *and* the sense reversed: it listed exactly the files that did
   **not** match. Affects save-game and custom-level listings.

3. **`main.c`: MSVC got no command-line parsing at all.** Everything was inside
   `#if !defined(_MSC_VER)` because MSVC lacks `getopt_long`, so the Windows build ignored
   every switch and was locked to fullscreen.

4. **Unreachable feature flags.** `WantCDRom` and `WantJoystick` both defaulted to 0 while
   their only switches (`-c`, `-j`) *also* set 0 — so music and controllers could never be
   enabled. Worth grepping for more of this pattern.

5. **`fmv.c` RGBA/RGB mismatch.** `UpdateFMVTexture` builds four bytes per pixel but uploaded
   with `GL_RGB`, three bytes per pixel, so every row was read skewed. Invisible while the
   monitors only ever showed random static.

6. **`CMakeLists.txt`** did not link `shlwapi` (`PathMatchSpecA`) or `winmm` (`timeGetTime`).

7. **`GotAnyKey` is declared with the wrong type in `fmv.c`** — `extern int`, when the
   definition in `win95/io.c` is `unsigned char`. Writing through the wrong declaration
   clobbers four bytes of the adjacent `KeyboardInput` array.

## 6. Gamepad

Menus, Marine movement and look, pause/resume, firing, jumping and interacting are
user-confirmed working. What remains unverified is listed in the follow-up below and in
§8 -- read those before assuming any part of this is still broken.

`src/access/acc_pad.c` uses SDL3's gamepad layer (not raw `SDL_Joystick`, whose numbering is
device-specific). Axes are written into the `JOYINFOEX` the engine already reads:

- left stick → `dwXpos`/`dwYpos` (strafe / forward-back)
- right stick → the **trackerball** axes `dwUpos`/`dwVpos` (turn / look)

The trackerball path multiplies by a sensitivity of 32 and was written for small relative
deltas, so a full-range stick must be scaled down (~/16) or the player spins on the spot.

Buttons publish as the engine's bindable `KEY_JOYSTICK_BUTTON_1..16`. Cursor keys and
Enter/Escape are published **only while a menu is up**, so the same buttons stay free for
in-game bindings.

Two bugs found in the first pass. The first has since been **superseded** -- see the
follow-up below, where pad and keyboard state are tracked separately rather than the pad
merely owning its keys:
- The pad cleared its keys every frame it did not press them. Since it is read *after*
  keyboard events, it wiped `KEY_UP`/`KEY_DOWN`/`KEY_CR`/`KEY_ESCAPE` right after the
  keyboard set them — so plugging in a controller broke keyboard menu navigation. It now
  tracks which keys it owns.
- **Profiles silently undo controller configuration.** `avp_userprofile.cpp` does
  `JoystickControlMethods = UserProfilePtr->JoystickControlMethods`, so settings applied at
  startup vanish the moment a profile loads — and any profile saved before controller support
  existed has the right stick disabled. Settings are now re-asserted every frame from
  `AccPad_ApplyControlMethods()` in `usr_io.c`.

### Controller follow-up — 2026-09-07

The user reproduced unresponsive menus with an Xbox Bluetooth controller. One SDL
probe saw no mapped controller; a later fresh probe detected the Xbox Series X
Controller and XInput reported changing input. This suggests connection timing,
but does not establish the cause of every earlier failure. The old engine opened
controllers only at startup and ignored SDL gamepad add/remove events.

Changes now implemented:

- `CheckForWindowsMessages()` handles gamepad connection/removal events. A pad
  that appears after startup can open, disconnect releases its synthetic keys and
  centers its axes, and another connected pad can take over. SDL subsystem setup
  is not repeated on every rescan. The raw joystick fallback now opens an SDL3
  instance ID from `SDL_GetJoysticks()`, rather than the invalid device index 0.
- New controller key presses set `DebouncedGotAnyKey`, allowing loading prompts,
  death restart, and completion waits to see the same press edge as the keyboard.
- Start/B are combined before publishing Escape, so holding B produces one Back
  edge. Physical keyboard state is tracked separately so releasing a controller
  alias preserves the same key held on the keyboard, and vice versa.
- `AccMenu_BindingActive()` reads the current menu binding-capture state directly.
  A, B, and D-pad buttons bind as joystick buttons instead of Enter/Escape/arrows.
  Start still cancels capture. The binding scan also stops before the array bound.
- Menu-state countdown now advances even while no pad is connected, preventing
  stale menu aliases when reconnecting during gameplay.
- `--padtrace` reports startup, device enumeration/open failures, connection
  changes, button masks, menu/binding state, synthesized keys and any-key edges.
  It now records button-to-button changes even if another button remains held.

Verification: Windows rebuild succeeded. The actual controller C module passes
50 assertions across nine hardware-free regression scenarios; the prior code
failed 15 of the 40 applicable assertions. The rebuilt game's live trace detects
the Xbox Series X Controller and shows D-pad Up/Down and A reaching menu keys.
The user confirmed that menu navigation, selection, and Back now work with the
Xbox controller over Bluetooth. In the subsequent Marine test, the user confirmed
left-stick movement, right-stick look, and Start to pause / A on Resume Game.
The user subsequently confirmed Marine firing, jumping and interacting with the
new action bindings. Live reconnection, binding capture, loading/restart prompts,
and remaining action/character coverage still need verification.

Run `tests\controller\run_tests.bat` for the regression suite. During live menu
diagnosis, use `tools\run.bat -w --padtrace`: `mapped=0` means no mapped pad opened;
`buttons=2000` with `KEY_DOWN=1` means D-pad Down reached the engine. If keys reach
the engine but the menu stays still, inspect `ActUponUsersInput()` and
`InputIsDebounced` before changing the SDL mapping.

### Marine gameplay follow-up — 2026-09-07

All three inspected local profiles had the original keyboard/mouse binding tables,
with no joystick buttons assigned. The Marine secondary defaults now provide RT/LT
fire, A jump, B crouch, X operate, Y/LB next/previous weapon, RB flare, D-pad Up image
intensifier, and left-stick click walk. A selected profile migrates only when both
Marine secondary bindings match the legacy defaults and primary bindings match
the configured primary defaults exactly. Custom bindings are retained,
and the binary profile format is unchanged. New profiles and Reset to Defaults use
the same preset; primary keyboard/mouse defaults remain intact. The old secondary
mouse/numpad fire, middle-mouse jump, Enter operate, and mouse-wheel weapon shortcuts
are replaced by controller buttons; secondary numpad look controls remain. Predator and Alien
presets are still pending.

Joystick gameplay processing now uses the same input-focus/menu guard as keyboard
actions. Previously sticks could request movement while the console owned input or
multiplayer menus were open; single-player pause normally hid this by skipping the
world update. Start opens the pause menu; use A on Resume Game to resume, since the
root pause menu deliberately ignores Escape/Back.

`--padtrace` now also records active action bindings and gameplay movement/action
requests, including right-stick input. Windows rebuild and whitespace checks passed.
The controller bridge passes 240 assertions across 13 scenarios. The actual
`ReadPlayerGameInput` fixture passes 12 checks (the previous source fails the two
focus/menu checks), plus the Marine preset/migration checks, including save/reload
idempotence and 16,320 single-byte custom-binding preservation cases. Run
`tests\gameplay\run_input_tests.bat` for the input and preset checks. Deterministic
trace testing also covers throttling, neutral releases and short action presses.

The user confirmed Marine movement/look and Start/A pause/resume before this
action-binding update. The rebuilt game's live trace confirms the Marine preset
loaded, with A reaching jump, X operate, B crouch, RT primary fire and LT secondary
fire, as well as both sticks reaching movement/look. The user subsequently
confirmed that firing, jumping and interacting now work during Marine gameplay.
These actions are now verified in play as well as in the engine input trace.

### Spoken Marine status — 2026-09-07

During live Marine gameplay, H or Xbox View/Back (joystick button 9) requests one
immediate spoken readout. `AccAccess_CheckRequests()` (originally `AccStatus_CheckRequest()`) runs at the end of
`ReadPlayerGameInput()` after console/pause handling. It combines simultaneous
shortcuts and consumes only eligible press edges. Active custom primary/secondary
bindings take priority independently for each shortcut. Menus, console input,
death, demo playback, completed levels, and other characters suppress the request.
No profile layout or controller preset changes are needed for these shortcuts.

`acc_status.c` formats live player data: health/armor percentages use the Marine's
difficulty-specific starting values with HUD rounding (including the below-full
99-percent rule). Fixed-point arithmetic uses wide intermediates. Loaded ammo
rounds up without overflow, and spare magazines exclude the currently loaded one.
Pulse rifle grenades, flamethrower fuel/tanks, right/left pistol ammo, and selected
grenade-launcher types have separate wording. Grenade counts come from the equipped
weapon, not the potentially stale per-type stores. Melee weapons do not announce
ammo; Cudgel's incorrect Pulse rifle template name is overridden. Slot, weapon,
ammo and localized-text lookups are guarded, with fallbacks for missing data.

Requested speech uses `AccSpeech_Say(..., 1)`, so repeat requests work even when
status has not changed. `--padtrace` logs `ACCSTATUS: speech=<available> <text>`.
The status fixture compiles the actual module with mocked engine data and speech;
the gameplay fixture checks shortcut edges, suppression, and custom bindings.
Run `tests\status\run_tests.bat` and `tests\gameplay\run_input_tests.bat`.
Windows rebuild and whitespace checks passed. The actual status module passed 89
assertions across 12 scenarios; the gameplay fixture passed 33 checks, including
108 shortcut/binding combinations, plus the existing Marine preset checks.
The live game detected NVDA and logged requested status with ammunition changing
from 99 to 92 to 66 rounds after the user's shots, with health/armor, weapon and
spare-magazine values included. The user subsequently confirmed that the spoken
Marine status readout is audible and correct through NVDA.

### Marine motion tracker — 2026-09-07

The next implemented milestone adds directional contact beeps and a spoken
tracker request on T or Xbox D-pad Down (joystick button 14). The shared
`AccAccess_CheckRequests()` in `usr_io.c` preserves H/View status, custom bindings,
all existing gameplay gates and press-edge behavior. Status wins simultaneous
requests; eligible tracker edges are consumed so it cannot interrupt next frame.
Tracker speech also requires player dynamics for current position and heading.

`DoMotionTrackerBlips()` retains the original object filters, sweep crossings,
capacity, nearest-contact tie order and beep lock. It additionally returns the
world position of the contact that earned the original beep. The three distance
tones and 2D scan click retain their original cadence. After fading, `hud.c`
publishes value copies of the remaining blips' world X/Z coordinates. Speech
uses these existing contacts, not an independent scan or saved object pointers.

`acc_tracker.c` formats the nearest contact still ahead and inside tracker range.
Range/nearest selection and approximate meters use the HUD's `Fast2dMagnitude`
metric (max + integer min/3); Euclidean distance would incorrectly reject some
visible diagonal contacts. Widened coordinate arithmetic prevents subtraction
overflow. Heading is 4096 units per turn, zero toward +Z, 1024 toward +X. Clock
bearings are rounded to the nearest hour; a contact below one meter is described
as within one meter. Empty valid snapshots say no contacts ahead; invalidated
snapshots say the tracker is unavailable. Distances are approximate, and existing
blips can persist briefly after an object stops moving.

`AccTracker_PlayContact()` uses `Sound_Play` format `nevm`: copied 3D data, external
handle, explicit volume, and Marine-AI ignore. The `m` flag matters because Marine
AI otherwise hears newly positional feedback. Inner/outer radii are two/three
times tracker range to avoid attenuating ordinary detectable contacts. Y is
flattened to listener height for a horizontal bearing. 3D audio bypasses the
platform's 2D volume scaling, so this adapter applies that scaling exactly once
to preserve existing loudness. Missing listener/invalid spatial data falls back
to the original 2D cue. OpenAL handles listener rotation. The retail tracker
samples inspected in `common.ffl` are mono and suitable for spatial playback.

`AccTracker_ResetHUD()` clears blips, cached speech data, scan/previous scan,
delay, distance lock and live cue handle. It runs from HUD init/reinit/kill,
`Destroy_CurrentEnvironment()` (including lift world changes), single-player
pause, and inactive tracker frames. Tracker activity requires an alive Marine,
normal vision, game input focus, no menu/demo/completed level, no observer mode
and no attached facehugger. Image intensifier therefore makes speech unavailable,
matching the visual tracker. Subsequent live transition checks are recorded below.

Validation: the complete Windows build passed, as did 224 tracker-module
assertions across 11 scenarios, 45 actual-source HUD checks, and the gameplay
input/preset suite (including status regression, 108 tracker binding combinations
and the existing 16,320 custom-preset preservation cases). The build still reports
the pre-existing `NewWidth`/`NewHeight` warnings in `main.c`; this change does not
touch that path. Run `tests\tracker\run_tests.bat`,
`tests\tracker\run_hud_tests.bat` and `tests\gameplay\run_input_tests.bat`.
`--padtrace` records `ACCTRACKER: cue=...` and `ACCTRACKER: speech=...` for live
diagnosis. Automated audio checks inspect engine call parameters, not perceived
direction or loudness. The user subsequently confirmed that the tracker readout
works. The live trace shows repeated requests reaching NVDA with "No tracker
contacts ahead." This verifies the shortcut and audible empty-tracker response;
subsequent guided listening and actual-contact verification are recorded below.
Subsequent tracker transition checks are recorded below. Prior Marine status
is already user-confirmed above.

### Guided tracker listening diagnostic — 2026-09-07

`--trackertest` enters `AccTracker_RunListeningTest()` from `main.c` after normal
sound initialization and `LoadSounds("PLAYER")`, before menus or levels. The six
contacts are explicitly described as simulated; this is a repeatable listening
test, not a new gameplay detection mode. `acc_tracker_test.c` calls the production
tracker snapshot, announcement and positional-cue functions and uses the retail
samples through the normal sound manager/OpenAL path. No save/profile is loaded
or written by the diagnostic. Both Windows and getopt argument paths recognize it.

A/Enter/Space advances and B/Escape cancels. Advancing speaks and schedules two
beeps after 4.5 and 5.5 seconds. T/D-pad Down replays the current example's beeps
immediately and one second later, without speaking again. Advancing or replaying
cancels any old pending beeps. Left/ahead/right at 12 meters are followed by ahead
at 5 and 25 meters, then the same world contact as example 2 with a quarter-turn listener
rotation. The last example should again be heard to the left and spoken at 9 o'clock.

The diagnostic checks the sound system and all three samples before starting,
initializes a temporary view and nonzero frame time (input polling divides by it),
and restores view/species/frame time and clears test contacts on return. A failed
sound handle is reported as playback failure. The trace reports visited examples
and valid playback handles; it never labels those as a listening pass. Window
close uses the engine's existing immediate exit path.

The full Windows build passed. The installed actual-module fixture passed 355
checks covering all examples, timing, controls, cancellation, unavailable audio,
playback failure and state cleanup. The executable help lists the new option.
In the first headphone trial, the user heard speech but reported no beeps. The
trace contains advances and repeats, with no playback calls before the user closed
the window. Repeating originally restarted the 4.5-second delay. Replay now starts
a beep immediately to avoid that delay and make audio diagnosis direct. The
immediate replay regression checks pass.

In the revised headphone trial, the live trace recorded valid playback handles
for all six examples and all three distance-tone samples. The user confirmed
that the left-hand cue was audible, 12 o'clock was centered, 3 o'clock was on the
right, and the higher 5-meter and lower 25-meter tones were distinguishable.
The guided directional-listening test is therefore user-confirmed. Separate
verification with a real level contact is recorded in the following section.

Run `tests\tracker\run_listening_tests.bat` for the diagnostic controls/timing
checks and `tools\run.bat -w --padtrace --trackertest`
for the guided listening test. Live tracker transition results follow below.

### Real Marine tracker contact — 2026-09-07

The user completed an in-level test in a separate `-w --padtrace --debug` run.
After loading a Marine level, the uppercase console commands `GOD` and `ALIENBOT`
were used for setup. `ALIENBOT` creates a normal hunting Alien two meters ahead,
using real dynamics and the normal tracker detection/sweep path. `GOD` prevents
death but does not prevent health/armor decreasing. Debug mode disables game saves
and normal best-statistics/progression updates; this is a temporary test session.

The user confirmed the first Alien was created and the tracker reported it ahead.
The live trace recorded `Nearest tracker contact at 12 o'clock, about 2 meters.`
through NVDA and repeated `SID_TRACKER_WHEEP_HIGH` cues (ID 95) at changing world
positions. The user separately confirmed hearing both the tracker beeps and the
spoken contact report. Real in-level detection, contact speech and audible cues
are therefore confirmed, in addition to the earlier guided headphone comparisons.

A second `ALIENBOT` attempt reported module containment failure. That is the
existing spawn routine rejecting a point outside a valid level module, not a
tracker failure. The spawn also requires the Alien model already loaded by the
level; missing-model errors are a different failure. No engine changes were needed
for this test. Close the console and keep normal vision enabled before scanning.

A subsequent pause/intensifier/restart trial is recorded below.

### Marine tracker transitions — 2026-09-07

The user completed the remaining planned tracker transition checks in another
`-w --padtrace --debug` Marine session with a real `ALIENBOT` contact:

- **Pause/resume:** Start paused the game and the user confirmed the tracker beeps
  stopped. A on Resume Game restored play; after a fresh sweep, D-pad Down gave
  an accurate report. The trace also records fresh cue calls after resuming.
- **Image intensifier:** D-pad Up enabled it and D-pad Down spoke "Motion tracker
  unavailable." The user confirmed normal tracker reporting returned after
  switching the intensifier off. The trace records the unavailable response while
  enabled and valid "No tracker contacts ahead" responses after disabling it.
- **Restart:** The user selected Restart Mission from the pause menu. After the
  level restarted, D-pad Down said "No tracker contacts ahead." The user then
  re-entered `GOD` and `ALIENBOT`, and confirmed both the spoken contact report and
  audible tracker beeps returned, checking twice.

These results confirm the tested Marine pause/resume, vision toggle and mission
restart paths. The initial empty response after restart and successful fresh
contact detection are separate observations. They do not establish save/load,
lift transitions or every other inactive-player state.

For repeat tests, Restart Mission is three D-pad Down presses from Resume Game
(through Save Game and Load Game), and takes effect immediately without a
confirmation dialog. Debug mode leaves those menu entries present. Restart
removes the test Alien, restores normal vision and resets `GOD` off; re-enable it
before creating another test Alien. D-pad Down navigates the pause menu, so issue
tracker requests only after resuming, with a few seconds for a fresh sweep.

No source or executable changes were needed for this validation. The existing
automated/build results above remain applicable; this session adds user listening
and controller observations, not another automated test run.

### Sonar sweep — 2026-09-07

`src/access/acc_sonar.c`. Where the tracker answers "what is moving near me",
this answers "what shape is the room". On demand only -- R on the keyboard, or
D-pad Left (joystick button 15) -- dispatched from the same
`AccAccess_CheckRequests()` as status and tracker, with the same binding-conflict
checks and gameplay gates. Status wins a simultaneous press, then tracker, then
sonar; every edge is consumed regardless so a losing request cannot fire again
next frame.

Nine rays span 180 degrees ahead, 22.5 degrees apart, out to 8 metres. Range is
room scale deliberately: the tracker reaches 30 m, but beyond about 8 m a sweep
stops describing the room and starts describing the level.

Raycast conventions, which are easy to get wrong:

- `FindPolygonInLineOfSight(direction, position, useOnScreenBlockList, ignore)`
  needs `LOS_ObjectHitPtr` cleared and `LOS_Lambda` preset to the maximum range
  *before* the call; results come back in those same globals. Direction must be
  a unit vector scaled to `ONE_FIXED`, and **both vectors are modified**, so pass
  throwaway copies.
- Test `LOS_Lambda < range` for a hit, **not** `LOS_ObjectHitPtr`. World geometry
  shortens the ray without ever setting an object pointer, and walls are the
  entire point of the sweep.
- Cast from eye height (`Global_VDB_Ptr->VDB_World.vy`), not the player position,
  or the sweep describes the floor and whatever step the player is stood on.
- Heading is the same 4096-units-per-turn convention as the tracker: zero faces
  +Z, a quarter turn (1024) faces +X.

The first version reported only wall distances per sector. The user's verdict was
that it "does not report corridors -- it only reports how far away walls are",
which was the right criticism: knowing a wall is 3 m to the left is not what you
act on, knowing you are in a corridor is. It now names the space and follows with
the numbers: "Corridor ahead. Walls 2 metres left, 2 metres right", "Dead end.
Wall 1 metre ahead", "Wall ahead, opening left", "Open space". All eight
open/blocked combinations across the three sectors are named explicitly.

A sector counts as open when *any* of its three rays reaches full range. That is
deliberately generous -- better to mention a gap that proves shallow than to miss
a doorway -- and is the first thing to tighten if alcoves start reading as
openings.

Only three things are played, one per sector, left then ahead then right, 500 ms
apart. A ping per ray at that spacing would take over four seconds; this lands in
about one. Walls use the three tracker pitches the player already knows (near
high, far low) rather than a second vocabulary for the same idea. Openings use
`SID_TRACKER_CLICK`, positioned out at the edge of range along the sector, so
"you can walk this way" never sounds like a distant wall. In the first version
openings were simply silent, which is why it seemed to report walls only.

Playback reuses `AccTracker_PlayContact()` rather than duplicating it, so the 3D
volume scaling and the Marine-AI `m` flag are handled in exactly one place.
`AccSonar_Reset()` is called from `AccTracker_ResetHUD()`: a sweep has the same
lifecycle as tracker state, so everything that invalidates one invalidates the
other.

Validation: Windows build clean with no warnings; 37 assertions across 13
scenarios in `tests\sonar\run_tests.bat`, which mocks the raycast and so can
describe a synthetic corridor, dead end or doorway ray by ray. The clock is
passed into both `AccSonar_Request()` and `AccSonar_Update()` rather than read
inside, so the ping schedule is deterministic under test. Those checks inspect
call parameters, not perceived audio.

The user confirmed live in a Marine level that left-to-right movement is
perceptible, the spoken summary is audible through NVDA, and after the shape
change that the sweep tells them what the space is. Not yet exercised: lifts and
moving geometry, very large open rooms, and whether the generous open-sector rule
holds up across the whole campaign.

## 7. Debugging notes

- Get real exit codes by running through a `.bat` that echoes `%ERRORLEVEL%`; PowerShell's
  `$p.ExitCode` came back empty for this process. `0xC0000374` is heap corruption,
  `0xC0000005` an access violation.
- PowerShell needs `&` before a quoted path; cmd and bash must not have it.
- `fflush(stderr)`-ed traces beat reasoning. The double-free looked like FFmpeg misuse for a
  good while; the dropped frames looked like duplicate source frames, then like a bad audio
  clock, before the real cause showed up.
- Cross-check claims about content against the source files with `ffprobe`/`ffmpeg` before
  concluding the code is wrong.
- **This machine sets `NoDefaultCurrentDirectoryInExePath=1`**, so cmd refuses to run an
  executable from the working directory unless it is written `.\name.exe`. It surfaces as
  `'name.exe' is not recognized`, which reads like a failed build rather than a refusal to
  execute. The test runners hit this and appeared to fail wholesale while the tests
  themselves were fine. Always invoke built binaries with an explicit path.
- **Keep the test suites dependent only on MSVC and PowerShell.** `tests/media` briefly
  needed Python to extract functions from source into a fixture header; the generated
  header is gitignored, so a machine without Python could not run that suite at all even
  though it had everything else. Rewritten as `extract_menu.ps1`. `tests/tracker` already
  did it this way.

## 8. Not yet done

- Remaining live controller validation: reconnection, binding capture,
  loading prompts, and other actions/characters. Menus, Marine movement/look,
  pause/resume, mission restart, firing, jumping and interacting are user-confirmed
  (see §6).
- Broader live tracker regression coverage, including save/load and lift
  transitions. Core Marine contact detection, speech/beeps, headphone direction
  and distance tones, empty responses, pause/resume, intensifier on/off and mission
  restart are user-confirmed (see §6).
- Broader live sonar coverage: lifts and moving geometry, very large rooms, and
  whether the deliberately generous open-sector rule holds across the campaign.
  Corridor, dead-end and opening naming, the ping timing and the wall/opening
  distinction are user-confirmed (see §6).
- Remaining gameplay accessibility work: route guidance to objectives, status and
  tracker support for Predator and Alien, and assisted targeting. §3 lists the
  engine primitives each would build on. Route guidance is the one that decides
  whether a level can be *finished* rather than merely navigated.
- Briefing audio for the plot messages is listener-relative, not positioned at the screen —
  `VolumeOfNearestVideoScreen`/`PanningOfNearestVideoScreen` exist in `fmv.c` but are never
  set or read by anything.
