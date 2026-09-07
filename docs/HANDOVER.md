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
    tests/controller/  standalone input regression checks
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
immediate spoken readout. `AccStatus_CheckRequest()` runs at the end of
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

## 8. Not yet done

- Remaining live controller validation: reconnection, binding capture,
  loading/restart prompts, and other actions/characters. Menus, Marine movement/look,
  pause/resume, firing, jumping and interacting are user-confirmed (see §6).
- Remaining gameplay accessibility work: motion tracker as a 3D audio radar,
  raycast sonar, status support for other characters, assisted targeting, and route
  guidance to objectives. §3 lists the engine
  primitives each would build on. This is the work that decides whether a level can be
  *finished* rather than merely navigated.
- Briefing audio for the plot messages is listener-relative, not positioned at the screen —
  `VolumeOfNearestVideoScreen`/`PanningOfNearestVideoScreen` exist in `fmv.c` but are never
  set or read by anything.
