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

**Spoken menus.** Narration is driven by capturing the text the menu renderer actually
draws, so every element type is announced correctly by construction — label, role and
live value. Graphic-only entries fall back to their help string, and the profile list
reads real profile names.

**Spoken game text.** Every in-game message — objectives, pickups, plot text — is
spoken, hooked at the single point they all funnel through. Mission preludes are read
on the level-select screens.

**Marine status.** During gameplay, press **H** or the Xbox **View/Back** button
(the small button with two overlapping squares) to hear health, armor, weapon,
loaded ammunition, and spare magazines. Fuel, grenade types, and each pistol in a
dual-pistol loadout are described separately. Each press speaks once and interrupts
older speech. If either shortcut is already assigned to a gameplay action in your
Marine controls, that assignment takes priority; the other shortcut remains usable.

**Music and cutscenes.** Neither was implemented in the port. Both are restored by
decoding the Bink and Smacker files the retail release ships, via FFmpeg: the
soundtrack, the fullscreen intros and outros with picture and sound, and the in-game
wall-monitor briefings.

**Gamepad support.** SDL3's mapped gamepad layer provides Xbox menu navigation
and twin-stick movement/look. Controllers can connect or reconnect after startup.
A selects, B goes back, and Start opens the pause menu. Select Resume Game with A
to resume. Menu aliases are disabled while capturing a binding.

Marine has a default controller layout in the secondary bindings:

| Control | Marine action |
| --- | --- |
| Left stick / right stick | Move and strafe / look |
| RT / LT | Primary / secondary fire |
| A / B / X | Jump / crouch / interact |
| Y / LB | Next / previous weapon |
| RB | Throw flare |
| D-pad Up | Image intensifier |
| Hold left stick click | Walk (running is the default) |
| View/Back or keyboard H | Speak Marine status (when unbound) |

Unchanged older Marine binding sets receive this layout when their profile loads.
Custom binding sets are preserved; use the game's control settings to assign their
controller buttons. Primary keyboard/mouse bindings stay available. Predator and
Alien action buttons still need manual bindings. See the handover for live-test status.

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

Flags added for testing without having to play to the content in question:

| Flag | Purpose |
| --- | --- |
| `--movie <file>` | Play one FMV and exit |
| `--plotmsg <n>` | Decode one wall-monitor briefing and report on it |
| `--padtest [secs]` | Report what SDL sees from the controller |
| `--padtrace` | Trace how pad state reaches the engine |
| `-intro` | Re-enable the startup logo sequence |
| `-w` / `-f` | Windowed / fullscreen |

Note that MSVC has no `getopt_long`, so the Windows build previously ignored *every*
command-line option; a hand-rolled parser was added.

## Controller regression checks

Run `tests\controller\run_tests.bat` to compile the controller module with mocked
SDL input and check menu press/hold/release, any-key prompts, overlapping keyboard
input, binding capture, and disconnect/reconnect behavior. It runs without a
controller or game window. Coverage also includes both sticks, trigger thresholds,
all button slots, and transitions from menus to gameplay.

Run `tests\gameplay\run_input_tests.bat` to check actual gameplay input, menu/focus
blocking, Marine preset migration, and custom-binding preservation. See
`tests/controller/README.md` and `tests/gameplay/README.md` for details.
Run `tests\status\run_tests.bat` to check spoken status against engine data,
including percentage rounding, ammunition types, invalid state, and speech calls.
