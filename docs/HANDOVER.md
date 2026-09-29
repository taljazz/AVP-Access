# AVP Access — engine notes and handover

Written so that someone picking this up cold — human or another assistant — starts with
the map rather than rediscovering it. §0–§9 are reference: what exists, how it hangs
together, and what is left. §10 is the dated development log, oldest first, which holds
the evidence behind every claim in the reference sections.

The theme, if there is one: this engine is 25 years old and the Windows path of the port
is not continuously tested. Several bugs below had been dormant for years because nothing
happened to exercise them. **When something behaves oddly, measure before theorising** —
three of the four hardest problems in this project were misdiagnosed on first inspection.

**Before starting work:** the tree is shared with another assistant, so run `git status`
and `git diff` rather than assuming it matches this document. Then `tools\build.bat` and
`tests\run_all.bat` — all 24 runners should pass.

---

## 0. Status at a glance

Updated 2026-09-29. Evidence levels, strongest first:

- **Player** — the user confirmed it live with NVDA and/or the Xbox controller.
- **Replay** — an assistant drove the real game through the bridge (`docs/BRIDGE.md`) and
  observed it working. Bridge runs are usually muted, so they say nothing about how cues
  *sound*; runs in survey mode make the player immortal, so they prove a route, not that
  the fights along it can be survived.
- **Tests** — covered by the regression runners only. Those compile the real module against
  mocked engine data and inspect call parameters, never perceived audio.

| Feature | Marine | Predator | Alien |
| --- | --- | --- | --- |
| Spoken menus, briefings, profile names | Player (species-agnostic) | ← | ← |
| In-game message speech (`NewOnScreenMessage`) | Player | Replay | Hook is species-agnostic; not checked |
| Message history (F1 / R3) | Replay (empty history); populated history Tests | Tests | — |
| Cutscenes, music, plot videos, menu background | Player / Replay (species-agnostic) | ← | ← |
| Controller in menus | Player | ← | ← |
| Controller gameplay preset + migration | Player | Tests (equipment chords, migration) | — |
| Stick response curve | Player | shared | — |
| Status readout (H / View) | Player | Tests | — |
| Motion tracker (T / D-pad Down) | Player | n/a — speaks that it is Marine-only | — |
| Sonar sweep (R / D-pad Left) | Player | shared code, not checked | — |
| Objectives (O / D-pad Right) | Replay | shared code, not checked | — |
| Route guidance (N / View+Right) | Replay: Derelict to completion (survey mode) | Replay: Waterfall to completion (survey mode) | — |
| Lift boarding/exit guidance | Replay (Derelict lifts, final shaft) | Replay (Waterfall final shaft) | — |
| Surveyed drops / jump prompts | Replay (Derelict room69 lip, shafts) | Replay (Waterfall fall room) | — |
| Optional jump assist (J / View+A) | — | Replay (Waterfall staging points only) | — |
| Combat targeting cues | Replay (encounters; kills and vertical aim not proven) | Tests | — |
| Supply guidance (L, K / View+Left, View+Down) | Replay (medkit collected) | Tests | — |
| Snap to guidance (View+R3) | Replay | Tests | — |
| Map export / audit (bridge `map`) | Replay (Derelict) | Replay (Waterfall audit) | untested |

Multiplayer is out of scope: every in-game accessibility feature is gated off in network
games.

## 1. Layout

```
APV Access/
  NakedAVP/      this repository — engine source + src/access/
    tools/       env, configure, build, run, run-fullscreen, stagedlls (.bat);
                 bridge.ps1 (bridge client), inspect-map.ps1, audit-map.ps1
    tests/       20 suites, 24 runners; run_all.bat runs them all
    docs/        HANDOVER.md (this), BRIDGE.md, MAP-INSPECTION.md
  game/          working copy of the retail data, lowercased (NOT in git)
  third_party/   SDL3, OpenAL Soft, FFmpeg (NOT in git)
  build/         build output, runtime DLLs, and every live-test log and
                 screenshot referenced in §10 (NOT in git — copyrighted material)
  .avp/          profiles and saves; run.bat always starts from the project root
```

The retail install is left pristine; `game/` is a copy. Data files and folders must be
lowercase — the port expects it.

**Staging trap:** do not exclude `*.dat` when copying game data.
`fastfile/aliensound.dat`, `marsound.dat`, `predsound.dat` and `queensound.dat` are the
species sound banks, not installer junk. Likewise the `FMVs/` folder is not optional —
the *music* lives there, as Bink files.

**Licence:** the AvP source is Rebellion's, released for non-commercial use only. The
README's credit and licence text must stay. Never commit game data, map exports,
screenshots or profiles.

## 2. The accessibility layer

All new code is in `src/access/`, kept separate from engine source. Each module degrades
to no-ops when its dependency is absent, so other platforms still build.

| File | Role |
| --- | --- |
| `acc_speech.*` | Tolk screen-reader output, loaded dynamically |
| `acc_menu.*` | Spoken menus, via render-text capture |
| `acc_media.*` | Bink/Smacker music, cutscenes, plot messages, menu background (FFmpeg) |
| `acc_pad.*` | SDL3 gamepad: hotplug, stick curve, menu aliases, `--padtrace` |
| `acc_status.*` | Spoken Marine and Predator status |
| `acc_objectives.*` | Objective cycling and stored mission-message history |
| `acc_tracker.*`, `acc_tracker_test.*` | Marine tracker cues/speech; `--trackertest` listening diagnostic |
| `acc_sonar.*` | Nine-ray sweep that names the shape of the space |
| `acc_route.*` | Continuous guidance: orchestration, speech, beacon, loot/combat hand-off |
| `acc_route_targets.*` | Which switch, door control or area an objective needs |
| `acc_wayroute.*` | Paths through authored intra-room waypoint volumes |
| `acc_traversal.*` | Local steering, detour grid, strafe/jump prompts, floor following |
| `acc_lift_route.*` | Platform lift boarding, waiting and exit |
| `acc_fall_route.*` | Hand-surveyed drops and approaches (Derelict, Waterfall) |
| `acc_jump_assist.*` | Optional Predator jump assist at surveyed Waterfall staging points |
| `acc_combat.*` | Visible-hostile targeting cues and aim corrections |
| `acc_loot.*` | Useful-supply browsing and ranking |
| `acc_snap.*` | One-shot turn to the active guidance point |
| `acc_bridge*.*` | File-based automation bridge for live testing (`docs/BRIDGE.md`) |
| `acc_map.*` | Read-only whole-level export (`docs/MAP-INSPECTION.md`) |

Engine files touched outside `src/access/` are listed where each hook is described (§3,
§10). The main integration points are `usr_io.c` (every shortcut, the controller presets
and profile migration), `hud.c` (message and tracker hooks, lifecycle resets),
`avp_menus.c` (menu narration), `triggers.c` (shared activation check), `bh_plift.c`
(lift hold) and `missions.cpp` (objective identity).

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

The animated menu background (`menubackground.bik`, 640×380) runs on a second,
nonblocking looping decoder that draws into the same software surface *before* the menu
text, without presenting; the normal menu renderer then adds its overlay and flips. All
77 retail movies were hash-checked and fully decoded in the 2026-09-19 visual/media audit
(§10).

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
user-confirmed working. What remains unverified is listed in §9, with the evidence in the
controller entries of §10 -- read those before assuming any part of this is still broken.
The stick curve was later softened (§10, Xbox sensitivity adjustment); the scaling below
describes the original mapping it builds on.

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
2026-09-07 controller follow-up in §10, where pad and keyboard state are tracked separately rather than the pad
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

## 7. Controls reference

Taken from `usr_io.c`, not from memory. Every accessibility shortcut yields to the
player's own bindings: if a profile binds the key or button to a game action, the
game action wins. In-game shortcuts work for the Marine and Predator only, in
single player, while alive and not paused.

**Menus** — arrows or D-pad to move, Enter or A to select, Escape or B/Start to go back.
F1 speaks the item's help string, F2 repeats the current item.

**In game**

| Action | Keyboard | Xbox |
| --- | --- | --- |
| Status readout | H | View, tapped and released alone |
| Motion tracker (Marine) | T | D-pad Down |
| Sonar sweep | R | D-pad Left |
| Cycle objectives | O | D-pad Right |
| Mission-message history | F1 | R3 |
| Route guidance on/off | N | View + D-pad Right |
| Browse useful supplies | L | View + D-pad Left |
| Guide to selected supply / cancel | K | View + D-pad Down |
| Snap facing to guidance point | — | View + R3 |
| Jump assist (Predator, Waterfall staging only) | J | View + A |

Holding View and pressing any other button cancels the status readout, so chords
stay quiet.

**Xbox gameplay presets** (secondary bindings; keyboard/mouse primaries unchanged)

| Button | Marine | Predator |
| --- | --- | --- |
| RT / LT | Primary / secondary fire | Primary / secondary fire |
| A | Jump | Jump |
| B | Crouch | Crouch |
| X | Operate | Operate |
| Y / LB | Next / previous weapon | Next / previous weapon |
| RB | Throw flare | Cycle vision mode |
| D-pad Up | Image intensifier | Cloak |
| L3 | Walk | Walk |
| R3 | Message history | Message history |
| View + RB / LB | — | Zoom in / out |
| View + X / B / Y | — | Recall disc / medicomp / grappling hook |
| View + L3 | — | Taunt |

Start pauses; use A on Resume Game (the root pause menu ignores Back). Profiles migrate to
these presets only when their bindings exactly match an older default, so customised
profiles are never overwritten. See §6 and the 2026-09-20 Predator entry in §10.

**Command-line switches** (`main.c`) — `-w` windowed, `-f` fullscreen, `--padtrace`
controller and shortcut trace, `--padtest`, `--trackertest` guided tracker listening
test, `--movie <path>` plays one FMV and exits, `--plotmsg <n>` plays one plot video,
`--skip-intro` skips the startup logos (they play by default), `--debug` enables the
console (`GOD`, `ALIENBOT`), and `--bridge` / `--bridge-dir` / `--bridge-audible` for
the automation bridge (`docs/BRIDGE.md`).

## 8. Debugging notes

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

## 9. Not yet done

Rewritten 2026-09-29 from the open items in §10. Ordered roughly by how much each
one stands between a blind player and finishing the game.

**1. The Alien campaign has no in-game support.** Menus, briefings, cutscenes and
spoken on-screen messages work for every species, but every in-game shortcut in
`AccAccess_CheckRequests()` is gated to Marine and Predator, and there is no Alien
controller preset. Cheapest first: an Alien status readout (health only; no ammo),
an Xbox preset with the same exact-match migration rule, and enabling sonar and
objectives, which are species-neutral. Route guidance is a separate, much larger
problem — the Alien climbs walls and ceilings, and the room graph's
`alien_only` links (§10, 2026-09-20 vertical passage) are exactly the ones Marine
routing refuses.

**2. Only the first level of each campaign is proven.** Route guidance is generic
code, but it has been walked end to end only on Derelict (Marine) and Waterfall
(Predator), and both depend on hand-surveyed helpers in `acc_fall_route.c` for the
drops and lips the NPC graph cannot describe. Each further level needs a map
export and audit (`tools/audit-map.ps1`), a replay, and surveys for whatever
vertical transitions the audit flags. Never globally enable `alien_only` links for
the Marine or Predator.

**3. Player verification backlog.** Built and replay-tested, but not yet confirmed
by the user with NVDA and the controller:
- Predator: status, vision/zoom speech, the View equipment chords, R3 history, jump
  assist, and the migrated saved profile's pad actions in play.
- Marine: route guidance timing and wording after the "less eager / shorter speech"
  tuning, the override approach, lift exit timing, supply guidance, snap (including
  combat pitch), View-release status.
- Combat cues against a live enemy — no kill, acquisition or vertical aim
  correction has been observed.
- Corrected species labels, the briefing, and sonar with the intensifier on.
- Populated message-history replay, profile editing, controller reconnection,
  binding capture, and loading/restart prompts.

**4. Survivability is unproven.** Both completion runs used survey mode (player
immortal, route combat takeover off). The normal-damage Derelict replay died in
combat. Whether the combat cues let a blind player win the fights along the route
is the open question.

**5. Known navigation gaps.**
- COMM-ENT04 staircase (Derelict) needed a manual jump the traversal prompt did not
  identify.
- After activating the glass-covered control, one run reported no clear local
  approach; moving door versus over-conservative clearance was never determined.
- Sonar's generous open-sector rule, lifts and very large rooms; eye-height clear
  rays do not prove a walkable route.
- Grid-search cost in busy scenes has not been observed.

**6. Smaller items.**
- Tracker across save/load and lift transitions.
- Plasma caster lock-on has no spoken cue.
- Plot-message audio is listener-relative, not positioned at the screen —
  `VolumeOfNearestVideoScreen`/`PanningOfNearestVideoScreen` exist in `fmv.c` but
  nothing sets or reads them.
- Bridge limits: timestamps are simulated time, not audible onset; FFmpeg voiceover
  and sound-stop events are outside the event stream.
- The engine fixes in §5 are worth offering upstream to `atsb/NakedAVP`.
- Pre-existing build warnings reported by earlier full rebuilds: C4700
  `NewWidth`/`NewHeight` in `main.c`, C4090 const mismatch.

## 10. Development log

Dated entries, oldest first, moved here verbatim from the sections they used to
interrupt. They are the evidence behind §0 and §9: what was built, how it was
tested, and what was and was not observed. Where an entry says its changes are
"local, uncommitted", that was true when written — all of it was committed on
2026-09-29. Paths such as `../build/...` and `build/...` are relative to the
repository and project root respectively and hold local, uncommitted evidence.

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

`AccTracker_PlayContact()` uses `Sound_Play` format
evm`: copied 3D data, external
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

### Existing-feature review — 2026-09-19

The current priority is refining existing accessibility before adding route
guidance or other features. This section supersedes earlier claims about complete
menu coverage and shared tracker/sonar/objective lifecycles.

Changes made during this review:

- Fixed ungrouped `if ... echo ... & exit` in `tools/env.bat` and `tools/run.bat`,
  which could return before setup/launch even when the condition was false.
  Fixed the equivalent `goto` guard hiding the missing-Tolk notice in staging.
  The launcher now preserves the game's exit code. `run.bat --help` was exercised.
- Single Player graphics now use the destination's localized species name.
  The shipped help strings were the same general instruction for all species;
  the earlier fallback did not identify the selection. Live logs now show
  Alien, Marine, Predator at positions 1, 2, 3.
- Capture ends after each rendered element and is cleared on menu setup, avoiding
  unrelated rendered text and prior-menu labels. The comparison cache omits the
  one-time menu title, eliminating the next-frame duplicate interruption.
- The briefing accessor now recognizes the actual BASIC/BONUS mission briefing
  screens. Previously it recognized episode selectors, where briefing data was
  blank. The basic Marine briefing was visually compared with its complete
  speech-request log after the fix.
- Objective cycling no longer resets whenever the tracker becomes unavailable.
  InitHUD, ReInitHUD and KillHUD reset the cursor; ordinary pause/vision tracker
  resets preserve it. Sonar is no longer canceled by intensifier mode alone.
- Repaired tracker/gameplay fixtures for the bridge's new dependencies and added
  cue-scope/clock assertions. Added focused menu regression fixtures.
- Bridge decimal parsing rejects overflow before multiplication on Windows and
  rejects trailing empty keys. Screenshot writes check I/O failure. Switching
  to step mode accounts for preceding real time. The held-time loop now honors
  queued window-close/quit events instead of merely pumping them.

Objectives and history from the earlier unfinished work remain included:
O / D-pad Right cycles visible objectives and explicitly reports missing text;
F1 / R3 requests stored mission-message history. Default-pad migration fills R3
only for matching older defaults, preserving custom configurations. The opening
Marine objective was verified live as one incomplete objective with no description.
R3 produced no speech in the tested opening state: no stored history was present.
Replay of populated history and old-profile migration still need live validation.

Verification evidence:

- Controller, tracker, listening diagnostic, status, gameplay input, tracker HUD,
  sonar, media, objectives and bridge runners passed this review. HUD has 48
  checks; bridge has 161 C assertions plus five independent PNG decode checks.
  The new menu suite passed 13 assertions across seven cases and documents its
  mocked boundaries in `tests/menu/README.md` (11 runners passed overall).
- The executable rebuilt successfully. A rebuild that recompiled main/frontend
  also reported existing C4700 NewWidth/NewHeight warnings and C4090 const mismatch;
  this is not a warning-free-build claim.
- Live bridge sessions captured profile selection, all three species, briefing,
  intro movie frames, loading and Marine gameplay. A 200 ms forward command changed
  player position; a requested 30-degree turn reported 31 degrees, with updated
  screenshots. Quit commands closed the launched processes.
- With intensifier active, sonar emitted three accepted sound starts at simulation
  times 25173, 25673 and 26173 ms. Logs included source, sample, volume, pitch,
  position, horizontal distance and bearing. Tests were muted: no new claims about
  perceived direction, NVDA clarity or controller feel are made.
- Local screenshots/logs are under sibling `build/review-2026-09-19/`, outside git.
  No retail assets are included in source changes. No commit/push during this review.

Remaining refinement work before new gameplay features:

1. User listening check of corrected species labels, briefing and sonar with
   intensifier. Check populated message history, profile editing and reconnect.
2. Sonar labels remain a nine-ray heuristic: an eye-height clear ray does not prove
   a walkable route; mixed wall/open sectors currently play the wall tone. Preserve
   the user-confirmed behavior until a targeted observation supports changing it.
3. Bridge runtime is experimental. A repository client does not exist yet;
   the review used a temporary local client and corrected the header's stale claim.
   Speech events are requests (also logged when muted), sound events are accepted
   engine starts, and `t` is simulated time in step mode. These are not measured
   speaker/NVDA onset times. Sound stops, FFmpeg voiceovers, and every native tracker
   scan click are not all captured by the accessibility-only event filter.
4. Validate held-time OS-close behavior live, bridge file-I/O failure recovery,
   populated history, save/load, moving geometry, and the remaining subtitle paths.
   Core parser/runner tests do not establish all of the engine integration behavior.

### History and bridge refinement — 2026-09-19, follow-up

User said the preceding review looked good and asked to proceed. This is not
recorded as a specific new listening result. Work remained on existing features.

F1 and R3 now respond "No mission messages yet." when history is empty. Both
were exercised against the actual opening Marine level and emitted that speech
request through the existing on-screen message hook. New messages become
immediately reviewable; browsing still goes newest-first with a four-second
timeout. History formatting is bounded and save-block metadata is validated
before copying; the stored format remains unchanged. Populated replay/load is
covered by fixtures, not yet by a newly observed campaign message in live play.

The bridge now retains a failed reply and its unreported events, releases held
input, retries publication once per second, and blocks the next command until
publication succeeds. It preserves the prior complete reply instead of deleting
it during a sharing violation. Commands are consumed before execution to prevent
repeated movement on a deletion failure. Oversized or embedded-NUL commands are
rejected instead of executing a truncated prefix. Parser errors retain a valid
sequence number even when later tokenization fails. Startup requires writable
event and readiness files; readiness is published atomically and removed on normal
exit. Native tracker scan clicks now carry the tracker log tag too.

`tools/bridge.ps1` is now the repository client; see `docs/BRIDGE.md`. It serializes
cooperating clients and retains pending-command metadata after a timeout so the
next invocation cannot silently resubmit movement. Use one game per directory.

Verification:

- Bridge core: 161 C assertions plus five independent image decode assertions.
- Bridge runtime: 31 checks using actual SDL filesystem operations/Windows locks
  and mocked engine boundaries; queued SDL quit while time is held releases input.
- Client: 12 PowerShell checks against that headless runtime fixture, covering
  serialization, timeout protection, sequence/error matching and quit. The helper
  is named avp.exe for process validation but is not a retail game session.
- Tracker HUD: 49 checks including native scan-click tagging.
- History: 21 checks for empty/newest/backward/wrap/expiry behavior, reset,
  bounded long text, valid save round-trip and rejected malformed metadata.
  Final integration: all 13 runners passed and the executable rebuilt successfully.
- A live startup with a regular file in place of the requested bridge directory
  exited with code 1 and an explicit startup error before opening gameplay.
- In a live Marine session, locking reply.json caused a client timeout; a following
  movement request was refused. Unlocking allowed recovery, with simulation frame
  15 and player position unchanged. Quit returned successfully, the owned process
  exited, and ready.json was removed. Logs/screenshots are in sibling
  `build/refinement-2026-09-19/`, outside git.

Remaining limits: simulated timestamps are not measured audible onset. FFmpeg
voiceover and sound-stop/completion events are still outside this event stream.
The close path was tested through the SDL queue, not a new physical window-close
test. Full save/load, populated campaign-message replay, controller reconnect,
moving geometry and all subtitle paths still need live coverage. Bridge log write
failures after successful startup are not a durability guarantee for events.jsonl;
the reply ring is bounded. No route-guidance feature was added in this pass.

### User confirmation and publication — 2026-09-19

After the refinement pass, the user reported "everything works" and explicitly
authorized committing and pushing to `origin/main` (`taljazz/AVP-Access`). This is
general user confirmation, not a separate recorded result for every outstanding
live-test case above. The source, tests, tools and documentation are included;
retail data, built executables and local screenshots/logs are excluded. Pre-existing
local clang-tidy/configure tooling edits remain outside this accessibility commit.

### Continuous Marine guidance — 2026-09-19

User chose continuous guidance with an on/off toggle. `N`, or hold Xbox View
and press D-pad Right, toggles it. Single D-pad Right still cycles objectives;
custom bindings override the shortcuts. Keyboard and controller chord edges are
consumed once. Default is off; pause, death, unavailable gameplay, level teardown
and HUD reinitialization cancel it. Intensifier changes leave it alone.

Implementation: `acc_route.c/.h`, `acc_route_targets.c/.h`; transient C objective
identity accessor in `missions.cpp`, input dispatch/update in `usr_io.c`, resets
in `hud.c`. A one-second positioned tracker high tone is tagged `source=route`
in bridge events. Speech queues without interrupting plot speech. New rooms
announce on the next update; other changed directions/distances at most every
four seconds. Stable wording does not repeat. No automated player movement.

Targets are direct, currently eligible binary/link switch activators of visible,
achievable, unfinished objectives. Mission request must be on with DontComplete
clear. Area bounds are WORLD space (the switch code tests player position
directly); midpoint is used. Shape-less logical switches are not physical points.
Unmet linked prerequisites, nonzero security requirements, autoexec/always-on
and already-on switches are conservatively excluded. Among direct activators,
the nearest by straight-line distance is selected; this does not rank alternative
activators by path accessibility. No reverse chain through messages/timers/door
controls or mission alterations is inferred.

Route search uses a per-call queue sized to the level's AIMODULE count, with a
65536 cap and allocation failure handling. It does not reuse/mutate the engine's
100-entry NPC route queue. Existing entry points and current door admission rules
select the next room opening. Complete routes exclude Alien-only links. If no
complete route exists, a second topology search identifies the first blocked or
restricted connection and routes only to its near side. Wording explicitly says
closed door or unverified passage. This does NOT certify player jumps/lifts,
walkability within a room, trigger-volume midpoint accessibility, or a complete
campaign route. Use sonar for local geometry. The partial route uses the shortest
topological path, not a plan minimizing locks or solving them.

Live bridge evidence (muted, Training, no user listening confirmation yet):

- Derelict has 172 AI modules. The visible blank-description objective resolves
  to one direct area trigger near END, far beyond START. Its topology includes
  locked doors and Alien-only links, so a complete Marine NPC path does not exist.
- Partial guidance gave 4 o'clock / 7 metres from START. Following the cue with
  normal turn/walk inputs reached START-EN01, CORR-11 and CORR-13. Logged next
  directions changed to 10 o'clock / 5 metres and 3 o'clock / 2 metres. Screenshots
  and cue positions agreed with the opening corridor turns.
- Bridge controller chord toggled off and on, without status/objective double
  readouts. Running with guidance off produced no route cues. Pause/resume left
  guidance off. Owned test game exited normally; no game was left running.
- Evidence lives outside git in sibling `build/route-2026-09-19*`. Final change
  to announce newly entered rooms sooner is covered by the route fixture; that
  timing refinement was not separately re-walked live.

Target resolver was delegated to one GPT-5.6 Luna low worker; coordinator reviewed
the code, corrected the DontComplete fixture to test an ON request with that bit
set, and integrated it. Route tests cover a 130-module graph, cycles, directional
links, door/restricted boundaries, speech/cue schedule, missing targets and reset.
Gameplay/HUD fixtures cover the chord, custom binds, pause and vision behavior.
Final integration: application build succeeded; all 15 regression runners exited
zero, including 25 route checks and 15 target-resolver checks. `git diff --check`
passed. Changes remain local pending the listening check and publication decision.

Next validation: user listening check for useful ping timing and spoken turns.
Remaining route work includes door-control prerequisites, story subgoals and
manual-traversal boundaries. Do not label this initial guidance as end-to-end
campaign accessibility or mark route guidance finished.

### Xbox sensitivity adjustment — 2026-09-19

User reported movement and looking both felt wrong, then clarified: "Controls
are way too sensitive." `acc_pad.c` now rescales the 8000 dead zone to zero,
uses quadratic stick response and caps output at half the old full-scale input.
Positive/negative extremes use their own SDL endpoint for equal maximum speed.
`usr_io.c` skips its additional legacy 12000 threshold only when an SDL pad is
present; otherwise the fine movement would vanish and resume with another jump.
Menu thresholds, triggers, keyboard/mouse and profile sensitivity/inversion are
preserved. User confirmed the new curve: "Okay, that works well."

Read-only inspection of actual Player and SkyPulse profiles in root `.avp` and
`build/.avp` found look sensitivities 32/32, no movement/look inversion, and no
auto-centering. SkyPulse has RT primary / LT secondary; Player stores older
keyboard secondary bindings, subject to the existing narrow preset migration.
No profile files were edited. Build passed; affected controller and gameplay
runners passed (240 controller checks, 95 gameplay checks). The 15-suite run above
predates this sensitivity change; it was not unnecessarily repeated.

### Door controls and breakable covers — 2026-09-19

Continuous Marine guidance now enumerates eligible direct controls for live
proximity, lift and switch doors. It rejects unreachable controls before choosing
the closest reachable one. If a topology path ends at a door without a usable
control, it excludes that boundary and tries another path, up to 16 attempts.
Unresolved doors retain an explicit unknown-control message. Multi-stage switch,
security and story prerequisites remain outside this implementation.

`triggers.c` shares its read-only selection routine with route guidance. The
"Press Interact" prompt requires the same selected object, alignment, range and
visibility as actual activation; proximity alone is insufficient. Obstructed
controls identify breakable non-explosive static/furniture scenery from engine
data, without labeling arbitrary geometry or pickups as breakable. Other
obstructions get a neutral announcement. Guidance never fires or activates.

Live bridge verification used an isolated Player profile and checkpoint under
`build/review-2026-09-19`, not the user's profile/save files. The initial shortest
path reached DR-COMM01, whose incoming edge locks rather than opens it. An
alternative path reached DR-COMM03's control behind an emergency glass cover.
Normal movement reached the cover; X alone could not operate through it. Firing
broke the cover, the readiness announcement appeared, X activated the control,
and normal walking crossed DR-COMM03 into COMM-ENT01. A final-build replay
verified the new breakable-obstruction announcement followed by readiness and
activation. Evidence: sibling `build/door-2026-09-19*` speech/cue logs and shots.
Test processes were closed afterward. User listening verification remains pending;
this is not evidence of a completed campaign route.

MSVC build succeeded. All 16 regression runners exited zero, including interaction
16 and target resolver 48 checks. The final alternative-path regression then
passed with the affected route suite at 32 checks (the full run had 31). Evidence
is in `build/door-regressions` and `build/door-route-alternative.log`.
One GPT-5.6 Luna low worker supported the target resolver and fixtures; the main
agent reviewed the enumeration contract, integrated the feature, completed the
alternative-path regression and performed the live tests. Changes remain local.

### Local strafe and low-obstacle jump prompts — 2026-09-19

User reported getting very far with door guidance, then requested strafe left/right
and jump-here prompts. The troublesome spot was somewhere mid-level and could not
be identified visually by the blind player; no exact saved-location reproduction
was available. Do not claim this pass fixes that particular obstacle.

`acc_traversal.c/.h` adds read-only Marine geometry probes to continuous route
guidance. Nearby lateral alignment suggests a small strafe while preserving facing.
When facing the route but locally blocked, it checks a low jump candidate, then
sideways paths with forward clearance. Three lanes span the Marine radius plus
30mm; body-height rays and floor samples reject obstructions, steep/moving ground
and drops. Shared public LOS results are restored after every query.

Jump candidates require standing, grounded, nearly-flat standard-gravity movement
and normal weapon jump encumbrance. The lower obstacle probe is 550mm (above the
450mm auto-step threshold), with clearance above 850mm and up to standing height
plus 1800mm. Floor/overhead samples extend four metres; landing ground beyond two
metres must be near the original height. No gaps, high ledges or lifts are inferred
as jumpable. The wording requests a SHORT forward movement: a full running jump
can overshoot the sampled area. This is sampled geometry, not a swept-body jump
simulation or proof of safe passage for every possible movement input.

New movement instructions are announced on the next one-second guidance tick,
ahead of clock/distance wording; stable instructions do not repeat every tick.
Interaction prompts and unresolved boundaries in the current room take precedence.
No movement or jump is performed automatically, and controls are unchanged.

Live bridge test from the isolated Derelict checkpoint verified both left and
right strafe announcements. Following each with normal sideways walking reduced
the distance to the opening while yaw stayed unchanged. Evidence is in sibling
`build/traversal-2026-09-19`; the owned test process was closed. Jump geometry is
covered by synthetic tests, not yet by a real low-obstacle traversal or the user's
mid-level location. User listening/play verification remains pending.

Final MSVC build succeeded and all 17 regression runners exited zero. Route
integration has 38 checks; analytic traversal has 16. The ceiling fixture was
then tightened to allow standing clearance while rejecting jump headroom, and
the affected traversal suite passed again. Logs: `build/traversal-regressions`,
`build/traversal-final-fixture.log`, `build/traversal-build.log`. `git diff --check`
passed. One GPT-5.6 Luna low worker investigated movement constants and authored
the analytic tests; the main agent reviewed geometry assumptions, integrated,
refined and live-tested the feature. No commit/push was made in this pass.

### Automatic Marine targeting guidance — 2026-09-19

User requested automatic targeting mode for immediate hostiles. `acc_combat.c/.h`
is called by `AccRoute_Update` BEFORE its one-second throttle, while continuous
guidance is enabled. It scans at 100ms intervals; a visible supported hostile
inside 20m takes ownership of speech/cues. The current target can remain to 24m
and is retained unless another visible threat is substantially closer (1.8 ratio).
Identity is compared against live enumerated strategy pointers plus eight engine
name bytes; no retained entity pointer is dereferenced after a frame ends.

Hostile policy is separate from SmartTarget_TargetFilter, which also accepts
friendlies/projectiles. The explicit roster covers Alien, Predator, Predalien,
Queen, Facehugger and Xenoborg; Marine/Seal require an AI Target equal to Player.
Health, destroy flags and NPC death state are checked; facehugger/xenoborg dying
states are checked directly because NPC_IsDead does not cover those two states.
Unknown types, corpses and multiplayer are excluded. No automatic aiming/firing.

Acquisition checks a ray from the camera to the engine targeting point (usually
chest), including offscreen near actors with display blocks. Range is explicitly
checked. All public LOS results are restored. The camera matrix gives horizontal
and vertical instructions. "On target" requires a fresh ray using the engine's
current GunMuzzleDirectionInWS to hit the selected actor first, not an angle cone.
This is not a promise about recoil, spread, ballistic weapons or future movement.

Combat stops the route tone, emits a medium cue every 450ms while misaligned and
a high cue every 200ms when aligned, tagged `source=combat` in bridge events.
Spatial tones convey horizontal direction; speech provides vertical corrections.
Speech changes are limited to 700ms and repeated at four seconds if unchanged;
new targets announce immediately. Combat speech interrupts older TTS to prevent
stale aim corrections. Existing mission history can replay interrupted messages.
Loss of sight stops cues immediately; after 1.2 seconds without a visible target,
the mode announces route resumption and refreshes the route immediately. It never
claims that all enemies are dead. Existing route lifecycle resets cancel combat.

Live bridge smoke test traversed the saved Derelict corridor, operated the glass-
covered control and entered the communications area. No qualifying hostile was
encountered, so this does NOT verify a live kill, target acquisition, camera aim
feedback or perceived combat audio. Logs/screenshots: `build/combat-2026-09-19`.
The test process was closed. A staircase at COMM-ENT04 required a manual jump;
the conservative traversal prompt did not identify it (overhead/landing limits).
Keep that observed navigation gap for follow-up rather than claiming all jumps
are supported. User combat testing remains necessary.

MSVC build succeeded. All 18 regression runners passed: the 17 existing runners
plus the new combat runner (42 checks); route integration has 42 checks. Evidence
is under `build/combat-regressions` and `build/combat-build.log`. The main agent
reviewed and corrected the fixture's sphere size, ray normalization and rotated
camera test before accepting results. One GPT-5.6 Luna low worker supported NPC
research and the fixture; main owned policy, implementation, integration and final
review. The working changes have not been committed or pushed.

### Local approach steering — 2026-09-19

User screenshots and feedback showed guidance pointing toward an opening without
getting around intervening scenery. The former local strafe hint could disagree
with a beacon still aimed at the distant opening. `AccTraversal_Steer` now checks
the direct three-metre approach, then searches up to 45 intermediate candidates
on three rings (1.2, 2.4 and 3.6 metres). Three body-width lanes check standing
clearance and sampled flat floor support on both legs. A selected waypoint is
retained through turns/movement and revalidated for new obstructions; changing
goal, room, lifecycle or combat ownership clears it. Physical controls use a
1.6-metre stand-off, while nearby actual interaction/cover instructions retain
priority. Both route speech and the beacon use the intermediate point. Forward
evasion hints cannot contradict that point. With no local approach or supported
jump, an interrupting stop message replaces queued directions and no route cue
plays. The planner does not move or turn the player.

Live bridge test `build/steering-2026-09-19` followed two detours around CORR22
obstructions, continued through CORR23/CORR21/CORR19/CORR10 and reached the
glass-covered control. Shooting the glass produced the correct Interact prompt.
After activating it, the next update reported no clear local approach. The test
ended there: whether this was the moving door or overly conservative clearance
has NOT been determined. Do not claim the post-control transition is verified.
The owned game process was closed. No user saves were modified.

Limits: this is a bounded two-leg sampled search, not a navmesh or a continuous
collision proof. Narrow steps, stairs, slopes, pits and routes needing longer
detours can be rejected. The screenshots alone do not establish the user's exact
map position. User listening/route verification is still needed at their trouble
spot. Tests include 27 analytic traversal checks and 46 route integration checks;
one GPT-5.6 Luna low worker supported the traversal fixtures, reviewed by main.
MSVC build and all 18 regression runners passed. Evidence:
`build/steering-build.log` and `build/steering-regressions/results.json`.
These changes remain local, uncommitted and unpushed.

### User feedback: less eager steering and shorter speech — 2026-09-19

The user found obstacle routing too sensitive and announcements too verbose.
Initial direct-path lookahead is now 1.2 metres rather than 3 metres, with the
same Marine body clearance and floor checks. Candidate detour legs retain their
longer checks. A cached detour releases when the original three-metre approach
is clear, avoiding continued diversion after passing an obstacle. This trades
earlier warnings for fewer premature detours; it needs user validation at speed.

Spoken labels are shorter (Opening, Detour), with no "about". Strafe/jump prompts
stand alone, the blocked warning is "Stop. Path blocked.", and usable controls
say "Press Interact." Room/detour/strafe changes wait at least three seconds
between announcements; other changes wait six. Comparisons use the last spoken
state so a deferred change is not lost. Stop, new jump, actual interaction
readiness and release from a blocked state remain immediate. Ordinary route
speech still queues to preserve mission narration; stop interrupts. Stable
instructions remain silent. Beacon timing and combat guidance are unchanged.
MSVC build passed; the affected route suite passed 49 checks and traversal passed
31 cases. Evidence is in `build/route-tuning-{build,route,traversal}.log`.
One Luna low worker updated fixtures; main reviewed them and added explicit
six-second distance-throttle and standalone-strafe assertions. No live listening
verification of this tuning yet. Changes remain local and uncommitted.

### Override approach: retain orientation on uncertain scans — 2026-09-19

User clarified that they never reached the override switch: repeated obstacle
takeovers prevented approach. Exact map position remains unverified from the
night-vision screenshot. Failed local searches previously asserted a blocked path,
removed the beacon and bypassed speech throttling on blocked/clear transitions.
That conflated failure to find a sampled route with proof of an obstruction.

Unresolved scans now preserve the original target point, bearing and cue, with
"Check path." appended. Both failure and recovery obey normal speech throttling;
they do not interrupt narration. Successful checked detours still use their
intermediate point. Jump and actual interaction readiness retain priority.
This provides orientation, not permission to walk through walls or across gaps;
sonar remains useful. It does not solve all switch approaches or certify this
exact override location. This supersedes the earlier stop-and-silence policy.
Build passed, along with 53 route checks including original-target cue retention
and blocked/direct speech throttling. Logs: `build/override-guidance-build.log`
and `build/override-guidance-route.log`. A Luna low worker updated tests; main
reviewed them. No live approach verification yet; changes are not committed.

### Whole-level inspection foundation — 2026-09-19

User approved whole-map inspection to support future navigation for all species.
Added read-only bridge `map` command, `acc_map.c/.h`, schema documentation in
`docs/MAP-INSPECTION.md`, and PowerShell `tools/inspect-map.ps1` for text reports
and incoming switch-chain inspection. Exports include all AI room entries and
their render meshes, directed entries including Alien-only restrictions, active
objects, static/scenery/switch base meshes, switch requests/prerequisites and
supported door state/lock fields. Raw meshes retain local coordinates and owner
transforms; animated collision and species traversal are explicitly unverified.

Live Derelict export from the isolated review profile:
`build/map-2026-09-19/map.json` and `map-report.txt`. 172 room entries, 187 render
modules, 388 objects; 21,994 module vertices / 14,674 module faces. Including
object meshes, all 21,164 exported face index lists were checked. Consecutive
exports were identical and left frame/time/player pose unchanged. Menu invocation
was rejected. Initial export crashed on a room without render modules; exporter
now guards that engine door-query precondition, verified in the successful runs.
The final owned game process exited normally; user saves were not modified.

Room 123 groups COMM-01, egg12 and COMM-03 despite distinct vertical extents.
COMM-01's switch at (29607,-298,-58748) sends eight requests, including both zero
and one values whose meaning depends on the recipient. These support investigating the
internal geometry, not proof of the screenshot location or an override route.
Next: derive connected surfaces and a valid switch approach inside the grouped
room, then test it. No navigation behavior changed in this inspection task.

MSVC build and all 18 regression runners pass (`build/map-build.log`,
`build/map-regressions/results.json`). Bridge coverage: 163 core assertions plus
5 PNG checks, 38 runtime checks and 12 client checks. Runtime map tests mock the
exporter; the geometry/clock checks above used the actual game. One Luna low
worker implemented bridge integration/tests; main implemented exporter/report,
reviewed integration and performed live validation. Predator and Alien export
sessions remain untested. All work is local, uncommitted and unpushed; proprietary
exports remain outside the repository.

### Derelict map-based guidance — 2026-09-19

Implemented locally, not committed or pushed. This supersedes the preceding
inspection-only next step. The whole map has been exported/audited; **full level
completion and all alternate routes are not yet verified**.

- Map export now includes all authored intra-room waypoint volumes/links and
  platform lift states/endpoints. `tools/audit-map.ps1` reports 172 AI entries,
  337 directed room links (12 Alien-only), 127 volumes in seven complex rooms,
  seven platform lifts, no missing room entry points and no invalid references.
  Disconnected walking-volume pairs include Alien-only ceiling/crawling links;
  do not treat every such pair as a playable-route defect.
- `acc_wayroute` searches owned arrays, preserving NPC scratch state and link
  restrictions. Supplemental touching-volume edges require live body/floor
  probes. NPC centres can be inside scenery at Marine height: an inset 7x7
  sample chooses a checked standing point if needed. Failure leaves the original
  centre unverified, with local steering responsible for clearance feedback.
- `acc_route` retains an intermediate point until within 650 mm; source room,
  goal, lift state, vertical change and reset invalidate it. This avoids repeated
  reversals at overlapping authored volumes. Ordinary short obstacle triggering
  and speech throttling are retained; lift arrival is immediate, subsequent
  lift updates obey the ordinary throttle.
- `acc_traversal` retains the short two-leg search and adds a bounded 31x31,
  800 mm grid (400 expansions) for multi-corner cases. Long goals may receive
  only a checked local prefix, never an unchecked final edge. Cached legs are
  revalidated. Floor sampling is at most 300 mm apart; the old square footprint
  behind the start was removed to avoid trapping a round player against a wall.
  Stopped platforms can support departure; moving ones cannot. Lift exits use a
  longer initial clearance check than ordinary 1.2 m walking lookahead.
- `acc_lift_route` routes to same-room automatic platforms, recognizes actual
  floor contact, announces wait/exit, and checks arrival every 100 ms. **Gameplay
  assistance:** `bh_plift.c` pauses a selected single-player lift's return timer
  at the desired landing while the guided player remains aboard. It releases
  after stepping off, route reset/off, or target requery. This was needed because
  the original 1.5-second turnaround can expire before speech is finished. No
  automatic interaction or player movement was added to normal gameplay.

Live evidence: owned, muted bridge sessions in `build/level-routing-2026-09-19`,
using the isolated profile under `build/review-2026-09-19/.avp/User_Profiles`.
User profiles/saves were not modified. The initial CORR-15 checkpoint replay
reached the glass-covered control, operated it, climbed the COMM stairs, followed
its upper perimeter to the west platform, descended and operated the override.
Export confirmed that switch's state changed and its lift was disabled. The route
then continued through the egg room, LABEX control, VA corridors and VIEWA lift,
SKA rooms and their lift, then APORT corridors as far as **APORT-T03**, approximately
(63576,-18272,-291729). Several hostile encounters exercised combat takeover and
route resumption. They do not prove kills or correct vertical aiming. The replay
ended in death after health had fallen to 7%; **the remaining level, other puzzle
states and optional areas are still untested**. Three lifts were exercised;
SKA's final successful exit used the new landing hold. Earlier failures/oscillation
are retained in `walk.txt`/event logs; do not count every earlier attempt as a pass.

A final-build replay from the private SKA checkpoint confirmed the held lift and
supported exit into the next corridor, including the quieter arrival updates. All 20 regression runners and the final MSVC build are
recorded in `build/level-routing-regressions/results.json` and
`build/level-routing-build.log`. Key suites: route 67, wayroute 25, lift route 21,
traversal 45 checks. These are code/analytic-geometry checks, not listening tests.
One actual Luna low worker implemented supporting fixtures; main integrated and
reviewed production changes and conducted the live replay. Grid-search runtime
cost is bounded but still needs observation in busy scenes. No universal navmesh,
full campaign guarantee, Predator routing or Alien wall-crawl routing is claimed.
All owned game sessions were closed after testing. Next live test: the user's
normal Xbox/NVDA run, especially override approach and lift exit timing; then
continue the unverified latter portion of Derelict from a suitable checkpoint.

### Marine loot guidance — 2026-09-19

Implemented locally, not committed or pushed. The feature is scoped to
single-player Marine play. `L`, or Xbox View/Back + D-pad Left, browses useful
supplies with needed medkits first, then other useful supplies by distance. `K`,
or View/Back + D-pad Down, guides to the selected supply; using the same control
again cancels supply guidance. Collection and cancellation both return to the
prior guidance state. Medkits and armor are used automatically by touching the
pickup. Ammunition enters inventory; weapons still use the normal Y/LB weapon
selection and RT fire controls. Custom gameplay bindings take priority over the
supply shortcuts.

Live evidence is under
`C:/Coding Projects/My Projects/APV Access/build/loot-review-2026-09-19/events.jsonl`.
The main agent observed the live medkit path: a prior main-state check showed
health 90, the selected medkit was announced and reached, the pickup speech
request was logged, and a following state check showed health 100 with
"Collected. Mission guidance resumed." The log also records supply browsing,
supply guidance, a second selected weapon supply and cancellation back to mission
route guidance. This is main-agent observed bridge evidence, not user NVDA
confirmation.

Xbox shortcut chords were exercised through engine inputs in the bridge/client
flow; this confirms the request path at the engine boundary, not human controller
or NVDA listening confirmation. MSVC build succeeded:
`C:/Coding Projects/My Projects/APV Access/build/loot-build.log`. All 21
regression runners in
`C:/Coding Projects/My Projects/APV Access/build/loot-regressions/results.json`
exited zero, including the new loot runner with 33 checks and the updated
gameplay runner with 105 verified PASS assertions for View-left browse,
View-down guide/cancel and the rest of the gameplay input suite. The checked
owned test PID 20572 was gone. No game process was stopped during this cleanup
pass.

### Explicit snap to guidance direction — 2026-09-19

Local, built, uncommitted and unpushed. Hold View/Back then click R3 to snap
once to the active Marine guidance point. R3 alone remains history and L3 remains
walk; no preset or saved profile changes. Input consumes the chord before history
and respects custom bindings. Navigation uses the freshly computed route/supply
waypoint with level pitch; nearby controls and visible combat targets include
pitch. No automatic walking or firing. Combat identity, range and LOS are checked
again before snapping. Lost targets, unknown routes and lift waiting refuse a
snap. Sonar/tracker information alone is not a snap destination.

`acc_snap.c` updates yaw/matrix/previous orientation and ViewPanX, clamps Marine
pitch to +/-896, and suppresses old turning inertia and look input for that frame.
Route target refresh suppresses pre-turn bearing speech. Existing guidance reset
paths clear its stored snap point. Paused/dead/demo/multiplayer inputs are gated.

Six affected regression runners passed (see parent `build/snap-regressions`):
snap 17, route 83, combat 48; gameplay, loot and controller also passed. MSVC build
passed (`build/snap-build.log`). Final-build bridge replay in
`build/snap-final-2026-09-19` turned away from the CORR-15 opening, then View+R3
returned yaw to the opening with only "Facing target." R3 alone still requested
message-history speech; with guidance off the chord reported no guidance target.
The owned replay was closed. This is engine/bridge observation, not user listening
confirmation; live combat pitch and control interaction snapping remain to verify.

### View modifier speech suppression — 2026-09-19

View status now fires on a solo release, not initial press or a timer. Any other
controller button during that hold cancels pending status, including repeated
View chords and custom controller actions. Keyboard H remains immediate; custom
View bindings retain their edges. Pause/focus loss, death and controller disconnect
cancel the pending announcement. Existing bindings were not changed.

Build passed (`build/view-modifier-build.log`). The gameplay fixture passed 135
assertions, including delayed and repeated View+R3/Down/Left/Right, modifier release,
subsequent solo tap, pause and disconnect (`build/view-modifier-input-tests.log`).
This fix is local and built; user listening confirmation is still pending.

### Visual/media audit — 2026-09-19

The staged and original GOG FMVs were inventoried and fully decoded: all 77
files match SHA-256, with no missing files or decoder errors. This covers seven
640x360 logo/campaign movies, the 640x380 menu background, 53 128x96 plot videos,
and 16 music files with dummy 4x4 video (15 Bink tracks plus introsound.smk).

The original animated menu background was still stubbed out. It now uses a
separate, nonblocking, looping decoder, redraws the background before menu text,
and retains the static backdrop if unavailable. It does not advance gameplay or
replace menu music. The obsolete hard-coded 360-line crop was removed so the
380-line movie is preserved. Changes are local, not committed or pushed.

Audit scripts, per-file hashes/metadata/decode logs and captured frames are under
`../build/visual-audit-2026-09-19/` (local copyrighted material; do not commit).
All seven logo/campaign movies were played through the game with sample frames
inspected; the background movie also completed standalone playback. The first
logo attempt ended early and was retried successfully. Movie screenshots use the
engine software-surface capture; gameplay screenshots use the OpenGL framebuffer.
Marine CORR-15 was inspected fullscreen with HUD, world textures, prelude overlay
and image intensifier off/on. Dark normal lighting and green intensified lighting
are visibly distinct. This is not a complete playthrough of all three campaigns;
natural mission triggers and every level/special effect are not exhaustively verified.
Validation: all 53 plot-message diagnostics exited 0 with changing frames, nonzero
palettes and no guard timeouts. These diagnostics exercise the in-engine decoder,
not every wall-monitor placement. The build succeeded; menu/media tests passed
22 assertions plus the no-FFmpeg fallback API test. Restored menu screenshots
show animation after multiple loops and no retained old menu labels; 794 of 5076
sampled background pixels changed between late captures. Profile/main/load menu
navigation and speech worked, followed by loading Marine CORR-15. The post-build
logo movie was also rechecked after separating scaling from presentation.

The tests used isolated copies of the Player audit profile, not SkyPulse. Bridge
runs were muted, so audio stream decoding is verified but this audit does not
claim a new human listening or lip-sync check. Missing-file fallback is implemented;
the no-FFmpeg fallback was executed, but removal of the installed movie was not tested.

### Slot 2 VIEWD-05 reproduction and approach fix — 2026-09-20

User supplied SkyPulse slot 2. Replayed an isolated copy as slot 1 under
../build/passage-slot2-2026-09-20/profile. Original and copied save SHA-256 both
DCDA09695449E84F79723E93496FB6BB744E566216B250E6AD8B401D55E9E010.
The regular launcher writes diagnostics to its console, not a persistent file;
the recorded diagnostics here were reproduced from the copied save.

Confirmed saved location: VIEWD-05, room 69, (-45803,3390,-217209), yaw4040,
health22/armor26. Mission route target END168; restricted boundary is approach75
SHIPCOR-05 ->76 JOCKEY-01. The saved obstruction is several rooms BEFORE that
boundary. Baseline guidance pointed at the room70 portal(-46297,1606,-209698),
walked into a wall and stalled at(-45709,4046,-216821). The fixed-height local
search could not route around this room's rising/falling floor; authored walking
volume links also include NPC Alien-only flags.

Changes (local, built, uncommitted):
- Ground-following navigation checks three width lanes at <=300mm intervals,
  stable supported floor, <=250mm local rise/drop, standing clearance and headroom.
  Grid nodes retain measured floor heights. Jump-hint probes retain their old checks.
- A tightly scoped, manually surveyed lower-floor approach for Derelict room69 and
  the exact room70 exit backs out, crosses the floor, approaches the raised lip,
  and gives a forward-jump instruction only when grounded and facing its landing.
  It hands back to normal routing on the upper landing. It does not globally permit
  NPC Alien-only links, does not apply to other exits/levels or modify user movement.
- Ordinary openings before a restricted boundary say "Opening". The warning is
  reserved for reaching the boundary, rather than suggesting every earlier wall
  is the unverified passage itself.

Live evidence: bridge/ is the failing baseline; manual/ records the manual bypass
and forward jumps into the opening. That manual run later died in combat beyond
SHIPCOR-03 and is NOT a level-completion proof. ground-test/ shows ground-following
alone still unable to solve the lip. survey-test/ follows the new beacons from the
unaltered slot2 copy into SHIPCOR-01(-45438,1567,-207660), still health22/armor26.
The automated final follower walked continuously rather than pressing jump; the
jump action itself was exercised in manual/. Later SHIPCOR-05/JOCKEY/HOLE and shaft
vertical transitions remain unverified and unsupported by this specific fix.

Validation: route95 checks (including eight exact-survey scope/stage/facing checks),
traversal51 checks (six new ramp/ledge/headroom cases), wayroute, lift_route,
route_targets, loot, snap and gameplay input runners pass. Final build succeeded.
Logs/test results are in ../build/passage-slot2-2026-09-20/. Initial runner-name
lookup errors were corrected to run_traversal_tests.bat/run_input_tests.bat before
recording passing results. Owned game sessions closed. User Xbox/NVDA retest pending.

### Unverified vertical passage investigation — 2026-09-20

User reached an "unverified passage" in Derelict, with a close-up green surface
in the screenshot; no save was made there. The exact location and mission versus
supply mode are not confirmed. Do not label this particular passage fixed.

Confirmed code cause: RouteFrontier previously chose an Alien-only shortcut before
a Marine-compatible route through a closed, operable door. It now searches the
Marine-compatible graph first before reporting a restricted boundary. A new
regression failed on the old code and passes after the change. Existing tests
still reject traversing restricted edges. Route 87, route-targets 48, loot 33,
and snap 17 checks pass; MSVC build succeeds. Local, uncommitted, unpushed.

Important remaining limitation: projload.cpp derives entry.alien_only from
AdjacentModuleFlag_Vertical. This is an NPC graph restriction, not proof that a
Marine player cannot use a drop/vertical transition. Offline BFS on the exported
Derelict graph from VIEWA room 73 to END room 168 has NO completely unrestricted
path, even ignoring door state. The unrestricted-species shortest graph path
includes SHIPCOR-05 -> JOCKEY-01 -> HOLE and liftshft02 -> liftshft01. Their player
collision, landing, fall and lift behavior must be verified before navigation can
cross them. The preference fix does not solve those required vertical transitions.

When a restricted-boundary instruction is spoken, stderr now records level,
source room, approach room, restricted room and mission/supply mode. This uses
the existing speech deduplication gate, does not require --padtrace, and will
identify a future occurrence. No live session or user save was altered in this
investigation. Next: obtain/reproduce the actual stopping point and validate the
corresponding vertical transition, then add an appropriate drop/lift route with
regression coverage. Never globally enable alien_only links for Marines.

### Nearby loot approach correction — 2026-09-20

User reports the selected pickup is announced but the guidance cannot bring them
onto it. Found a confirmed final-approach defect: within the existing 1.2 m
horizontal/1.6 m vertical visibility check, the route replaced the bearing and
distance with only "Medkit. Walk into it." even for a pickup behind the player.
It now retains the clock direction and distance and appends "Walk into it.".
The beacon/snap point and confirmed collection hook remain unchanged.

Built locally, not committed/pushed. Route tests: 86 checks, including nearby
left/behind/ahead bearings changing with player yaw; loot 33 and snap 17 pass.
An isolated Player checkpoint was guided off the SKA lift to the medkit; runtime
confirmed "Collected. Guidance off." and health increased from 90 to 100.
Evidence: ../build/loot-check-2026-09-20/near/events.jsonl and walk.txt. This verifies
collection, while the left/behind last-step cases are fixture-tested; the user's
specific failing location still needs their live retest. SkyPulse's original files
were not modified. The root Play AVP Access.bat uses tools/run.bat -w and the same
build/avp.exe; use that windowed launcher per the user's confirmed visual preference.

### Derelict whole-map audit and exit survey — 2026-09-20

Local, uncommitted work. This extends the previous slot-2 fix and supersedes the
statement that the latter half has never been traversed. It does not certify
all optional rooms, all puzzle states, other levels, or Predator/Alien movement.

The entire exported graph was audited: 172 AI rooms, 337 directed connections,
no missing entries or invalid references, 127 authored waypoint volumes in seven
complex rooms, and seven platform lifts. The local per-room report records paths
for every room. With only the three surveyed downward shaft links allowed,
162 rooms have a topological path to END; 110 have a path through the snapshot's
currently AI-passable rooms. The other ten include disconnected entries and
restricted side branches. Neither count establishes physical player clearance.

The apparent restricted links 75->76->158 are the long SHIPCOR-05/JOCKEY/HOLE
platform descent. Link 171->159 is the final lift shaft. Route search now accepts
only these exact directed Derelict connections, matching world entry coordinates.
Reverse climbs and other NPC-restricted links stay restricted. Guidance uses the
lower landing goal rather than intermediate shaft portals, recognizes a moving
platform across AI-room boundaries, waits aboard, then guides the exit. A missing
identified platform produces a wait, not permission to walk into the shaft. The
final platform's origin is 1945 mm above the standing-player position; its boarding
check accounts for that measured offset only on this identified shaft.

Live evidence in `../build/full-route-2026-09-20/`:
- `live/`: normal-damage replay from a private slot-2 copy, through VIEWD-05,
  ship corridors, the long descent and HOLE, then death during Alien combat.
  Health/armor did not decrease during that descent.
- `survey1/`: opt-in survey run reached the actual Level Completed screen;
  `shot-000425.png` records it. Ordinary movement, turns and firing were used;
  no teleport, door unlock or objective manipulation. Enemies obstructing the
  path were cleared with firing. This run preceded the final-shaft guidance fix.
- `final/`: second replay verified explicit final-shaft boarding/wait/exit and
  reached completion again; the new one-time `Level complete.` speech was logged.
  It exposed a contact-gap lift exit trap, requiring a manual forward command;
  do not treat this run as verification of the subsequent contact-gap fix.
- `landing-check/`: replay with the contact-gap fix crossed both shafts and the
  final doorway without the previous rising-platform trap, then reached the
  completion screen (`shot-000446.png`) and logged `Level complete.` once. Aliens
  physically blocking the exit and later water walkway were cleared by firing;
  this was not an uninterrupted walking-only run. A jump attempted at the latter
  obstruction did not clear it; shooting the blocking Alien did.

The final lift hold is retained across a brief loss of floor contact only within
its surveyed lower step-off area. Leaving that area or resetting guidance releases
it. Other platforms still require contact. Final MSVC build succeeded. Nine
affected regression runners passed (`tests.json`); route 106, lift route 36,
traversal 51, and bridge runtime/client 49+12 checks. The last rebuild only removed
an unused lift variable and refreshed a header comment after the live replay;
the lift suite was rerun successfully. All owned game processes were closed.

Survey mode requires both the bridge and environment `AVP_BRIDGE_SURVEY=1`.
It makes the player immortal without healing, and suppresses route combat takeover.
It is off for normal launches; results establish navigation, not combat survival.
Only disposable profile copies were used. Original SkyPulse slot 2 SHA256 remains
DCDA09695449E84F79723E93496FB6BB744E566216B250E6AD8B401D55E9E010.
Map exports and screenshots stay outside Git. See `docs/BRIDGE.md` for survey limits.

### Predator Waterfall navigation — 2026-09-20 (implemented and replay-verified)

Local, uncommitted Predator support targets the first Waterfall mission only.
Implemented support includes player status, field charge, cloak/vision and selected
weapon speech; shared sonar/objective paths; Predator controller defaults and custom
binding checks (the final three active Predator configuration slots); combat/loot/height
handling; route/snap gates; and an exact Waterfall + Predator + room-94 opening-control
matcher. Status, sonar/objectives, combat, loot and species-aware traversal are shared
support. The Predator default controller preset assigns R3 to stored-message history;
View+R3 still selects snap, and only exact prior defaults migrate while custom bindings
remain intact. The opening-control matcher, surveyed fall-room run-up helper and
final-shaft lower-stair/lift itinerary are the Waterfall-specific route additions.
Marine behavior remains covered by
its existing regressions. A 112-room, 160-render-module, 227-link static
audit found 52 species-restricted links and no missing entries or invalid references.
The fall-route helper gives manual movement cues. An optional, explicitly started
jump assist uses normal movement requests, never writes physics state, and cancels
when the player takes control. Keyboard J (when unbound) and View+A (when View is
unbound and A retains default Predator Jump) start/cancel it only at the two surveyed
staging zones with route guidance on and the opening gate unlocked.

Predator equipment controls in the Xbox preset use View/Back chords: RB/LB zoom in/out,
X requests disc recall, B selects a possessed medicomp, Y requests the grappling hook,
and L3 taunts. Zoom reports its step (1–3) or normal view; each Predator vision cycle
speaks the resulting mode. Medicomp uses the regular fire controls after selection (RT
heals, LT extinguishes fire). The grappling hook remains equipment-gated, and recall
feedback confirms a request rather than a successful return. These chords borrow only
exact stock bindings, preserve custom View/button assignments, suppress the borrowed
standalone action while held, and allow repeated taps while View remains down. Automated
input and vision tests pass; this addition has not yet been live-tested with a controller
or screen reader. The plasma caster's engine lock feedback has no new spoken accessibility
cue in this change.

Saved-profile diagnosis found why one player's pad actions had been absent while the
sticks still moved: the existing Predator profile kept the old keyboard-only secondary
actions, and its custom keyboard primary differed from the active default-key table.
The old migration required a byte-for-byte primary match and compared the full secondary
struct, including two unused expansion bytes that were zero in the profile but `KEY_VOID`
in the legacy initializer. The focused migration now recognizes only the exact old
active 30-slot secondary layout, accepts a primary only when it has no gamepad bindings,
and preserves that primary and both expansion bytes. Any customized active secondary
slot or gamepad binding in the primary blocks migration. The existing profile is read
and upgraded in memory on load; no profile file was edited by this fix.
The focused gameplay-input runner now passes 204 assertions, including a migrated
saved-profile-shaped fixture for A jump, RT/LT fire, Y/LB weapon cycling, RB vision,
D-pad Up cloak, View+RB zoom and View+X recall. These are automated input-map checks;
they do not substitute for the player's live controller check.

Automated regressions and their logs are listed in
`../build/predator-regressions-20260920/SUMMARY.txt`. Disposable live surveys
verified the first switch, the two surveyed jumps, gate02 into tunnl01, and the
later route through the final shaft and airlock to the actual `Level complete`
event. Trace4 reached that event at sequence 1834; `bridge/events.jsonl` records
the speech and `bridge/shot-001836.png` records the completion screen under
`../build/predator-jump-assist-trace4-20260920/`. Stage4 checkpoints include
slot 2 at the upper start, slot 3 on the north deck, slot 4 at northeast staging,
and slot 5 on the east deck. Stage5's original profile remains preserved at
gate05 (slot 7 in the security area, slot 8 inside gate05). The earlier jump
assist clone profile is `../build/predator-jump-assist-replay-20260920/profile/.avp`:
slot 7 is after the upper switch and slot 8 is the pre-lift checkpoint in well01.
The trace4 session directory itself has bridge logs, not a profile. In that
clone, keyboard J completed the first
guided jump and View+A completed the second; both landed grounded. Start/cancel
and manual-control cancellation were also checked at staging. Earlier
`pad_l3+pad_r3` snap and `rshift` walking observations were invalid; controls
confirmed from `usr_io.c` are `w+lshift` to walk, `w+rshift` to jump, Space to
interact, and `pad_view+pad_r3` to snap. Survey immortality does not stop health
loss; completion in survey mode does not establish normal-combat survival.

The NPC graph has a directed room-9-to-94 edge but no return edge from 94 to 9.
The graph remains unchanged and no global waiver was added. Earlier physical
survey play crossed the intervening gates and reached completion; it remains
separate evidence from the targeted guided replay below. Switch6 unlocked gate03,
switch7 opened the water gate, and switch56 changed gate05 passability before its
physical crossing.

The latest route source was rebuilt with `tools/build.bat`. A targeted replay
started from the saved pre-lift slot8 checkpoint, not a fresh campaign. Frame 1
confirmed the player grounded in well01 at `(190386,32010,49019)`, switch11 state
0 and lift8 disabled at upper height -2076. With route guidance enabled, the
bounded stair approach led to the measured corner and then switch11; Interact
changed it to state 1. The player waited, boarded, rode the lift, followed the
north exit point `(188281,844,72090)` and west point `(182512,844,71959)`, activated
upper switch58 and traversed the airlocks. In
`../build/predator-final-guided-replay2-20260920/bridge-clean/events.jsonl`,
event 392 at frame 1220 says `Level complete.` The final reply (sequence 115,
after the owned process was quit) reports grounded in airlock03 at
`(178935,844,101040)`. The clone profile is
`../build/predator-final-guided-replay2-20260920/profile/profile/.avp`; it is
separate from the preserved stage5 profile and the earlier jump-assist clone.
This verifies the lower-lift-to-completion segment with the latest guided build,
not a newly guided full-campaign run.
The tested controls from `usr_io.c` are N or View+D-pad Right to toggle route
guidance, Space or Xbox X to interact, W+Left Shift to walk, and W+Right Shift
to jump. View+R3 snaps to guidance; keyboard J/View+A start the optional assist
only at the two surveyed opening-jump staging points.

Focused suites pass route 159 and lift-route 63. The route suite pins the lower
stair approach and passed-corner conditions; raw outputs are
`../build/predator-regressions-20260920/route-final-shaft.log` and
`../build/predator-regressions-20260920/lift-route-final-shaft.log`. The earlier
trace4 physical survey separately reached `Level complete` at sequence 1834;
the earlier opening jump-assist clone separately verified keyboard J and View+A
through both surveyed jumps. Room82 cue stickiness was not confirmed: distances
decreased 2356→1649→806 and the waypoint advanced. Survey mode suppresses combat
takeover and makes the player immortal. These runs therefore do not establish
normal-combat survival or normal combat success. NVDA speech quality and user
controller listening/feel remain unverified. Grounded/nearly-flat contact,
velocity and camera telemetry were observed. Static downstream notes remain in
`../build/predator-downstream-stage3-20260920/`.
