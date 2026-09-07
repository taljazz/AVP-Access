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

**Music and cutscenes.** Neither was implemented in the port. Both are restored by
decoding the Bink and Smacker files the retail release ships, via FFmpeg: the
soundtrack, the fullscreen intros and outros with picture and sound, and the in-game
wall-monitor briefings.

**Gamepad support.** SDL3's gamepad layer, so an Xbox controller works without
configuration, including menu navigation.

Several long-standing bugs in the engine were fixed along the way — see
[docs/HANDOVER.md](docs/HANDOVER.md), which documents the engine's traps in detail.

## Building (Windows)

Requires MSVC, CMake and Ninja (Visual Studio ships all three), plus SDL3, OpenAL Soft
and FFmpeg. FFmpeg is optional: without it the game builds and runs, simply with no
music or cutscenes.

```
cmake -S . -B build -G Ninja ^
  -DSDL3_INCLUDE=<sdl3>/include -DSDL3_LIBRARY=<sdl3>/lib/x64/SDL3.lib ^
  -DOPENAL_INCLUDE_DIR=<openal>/include/AL -DOPENAL_LIBRARY=<openal>/libs/Win64/OpenAL32.lib ^
  -DFFMPEG_ROOT=<ffmpeg>
cmake --build build
```

Copy `SDL3.dll`, `OpenAL32.dll` (OpenAL Soft's `soft_oal.dll`, renamed), the FFmpeg
DLLs and `Tolk.dll` next to the executable. Point `AVP_DATA` at your game data
directory, whose files and folders must be lowercase.

Linux and macOS should still build — the accessibility layer is guarded so that
platforms without Tolk or FFmpeg compile to no-ops.

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
