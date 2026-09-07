# AVP Access — engine notes and handover

Written so that someone picking this up cold — human or another assistant — starts with
the map rather than rediscovering it. Everything here was verified against the running
game, not inferred from reading.

The theme, if there is one: this engine is 25 years old and the Windows path of the port
is not continuously tested. Several bugs below had been dormant for years because nothing
happened to exercise them. **When something behaves oddly, measure before theorising** —
three of the four hardest problems in this project were misdiagnosed on first inspection.

---

## 1. Layout

```
APV Access/
  NakedAVP/      this repository — engine source + src/access/
  game/          working copy of the retail data, lowercased (NOT in git)
  third_party/   SDL3, OpenAL Soft, FFmpeg (NOT in git)
  build/         build output + runtime DLLs
  tools/         env.bat, configure.bat, build.bat, run.bat
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

## 6. Gamepad — current state and the open problem

`src/access/acc_pad.c` uses SDL3's gamepad layer (not raw `SDL_Joystick`, whose numbering is
device-specific). Axes are written into the `JOYINFOEX` the engine already reads:

- left stick → `dwXpos`/`dwYpos` (strafe / forward-back)
- right stick → the **trackerball** axes `dwUpos`/`dwVpos` (turn / look)

The trackerball path multiplies by a sensitivity of 32 and was written for small relative
deltas, so a full-range stick must be scaled down (~/16) or the player spins on the spot.

Buttons publish as the engine's bindable `KEY_JOYSTICK_BUTTON_1..16`. Cursor keys and
Enter/Escape are published **only while a menu is up**, so the same buttons stay free for
in-game bindings.

Two bugs fixed here:
- The pad cleared its keys every frame it did not press them. Since it is read *after*
  keyboard events, it wiped `KEY_UP`/`KEY_DOWN`/`KEY_CR`/`KEY_ESCAPE` right after the
  keyboard set them — so plugging in a controller broke keyboard menu navigation. It now
  tracks which keys it owns.
- **Profiles silently undo controller configuration.** `avp_userprofile.cpp` does
  `JoystickControlMethods = UserProfilePtr->JoystickControlMethods`, so settings applied at
  startup vanish the moment a profile loads — and any profile saved before controller support
  existed has the right stick disabled. Settings are now re-asserted every frame from
  `AccPad_ApplyControlMethods()` in `usr_io.c`.

**Status: unresolved.** SDL reads the controller perfectly (confirmed with `--padtest`: all
buttons and both sticks report), and `--padtrace` confirms the pad code runs each frame and
that menu state is detected. But the user reports the pad still does nothing in the game. The
two fixes above are plausible causes but are **not yet confirmed to fix it**. The next step is
`--padtrace` output taken while pressing the D-pad in a menu: `anyButton=1` with `KEY_UP=1`
means the pad reaches the engine and the fault is further in; `anyButton=1` with `KEY_UP=0`
means the gating is wrong.

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

## 8. Not yet done

- Confirming the gamepad fixes (see §6).
- The gameplay accessibility layer proper: motion tracker as a 3D audio radar, raycast sonar,
  status readout, assisted targeting, and route guidance to objectives. §3 lists the engine
  primitives each would build on. This is the work that decides whether a level can be
  *finished* rather than merely navigated.
- Briefing audio for the plot messages is listener-relative, not positioned at the screen —
  `VolumeOfNearestVideoScreen`/`PanningOfNearestVideoScreen` exist in `fmv.c` but are never
  set or read by anything.
