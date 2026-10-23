# KameoRecomp

<p align="center">
  <a href="https://discord.gg/XQtM5TmNs8"><img alt="Discord: join the server" src="https://img.shields.io/badge/Discord-Join%20the%20server-5865F2?style=for-the-badge&logo=discord&logoColor=white"></a>
</p>

A static recompilation of **Kameo: Elements of Power** (Xbox 360, 2005) to native Windows, built on the
[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk). The game's PowerPC code is translated to C++ and
compiled; the Xbox 360 GPU is emulated by ReXGlue's Xenos backend (derived from [Xenia](https://xenia.jp)).

> [!WARNING]
> Early work in progress: the project has just been set up. There are no releases.

> This project is not affiliated with Microsoft or Rare. It contains no game files and no game code: you need
> your own legally obtained copy of the game.

## Status

- `default.xex` is recompiled and reaches the title screen with the default settings (Direct3D 12,
  30 FPS). Gameplay has not been tested yet.
- `kameo.toml` starts with POTCAWERecomp's GPU settings (no frame wait for new pipelines, shader control
  flow without a switch, no copies into render targets a clear overwrites, CPU uploads kept valid across
  frames); whether Kameo needs each of them has not been tested yet. POTCAWERecomp's
  `render_target_draw_clears` is off: with it the first cinematic of Thorn's Castle shows no geometry.
- Fixes so far: one function the code analyzer missed (`kameo_functions.toml`), and the game's waits
  for the GPU and its video reader no longer keep one CPU core busy, and, as in GRAWRecomp and POTCAWERecomp,
  the generated code keeps the PowerPC registers in host registers ([`docs/cpu-usage.md`](docs/cpu-usage.md)).
- **60 FPS option**, with the automatic camera following fixed for it
  ([`docs/frame-rate.md`](docs/frame-rate.md)). Off by default; tested only in the first part of Thorn's
  Castle so far.
- **Ultrawide** (Hor+): on a screen or window wider than 16:9 the game sees more to the sides, and the HUD,
  menus and videos stay 16:9 in the center (both renderers, [`docs/ultrawide.md`](docs/ultrawide.md)).
- **Cinematics without black bars** (optional, at 16:9 and ultrawide; [`docs/cinematic-bars.md`](docs/cinematic-bars.md)).
- **Quitting from the title screen**: B on "PRESS START" asks whether to quit the game, A quits and B cancels
  ([`docs/quit-prompt.md`](docs/quit-prompt.md)).
- **Game language** chosen in the launcher: English, French, German, Italian, Spanish (Spain or Latin
  America), Japanese, Korean or Traditional Chinese ([`docs/language.md`](docs/language.md)).
- **Kameo Launcher** ([below](#the-launcher)): the first run from your ISO, language, monitor, fullscreen /
  borderless / window, window resolution, renderer, image format (16:9 / ultrawide), cinematics without black bars,
  internal resolution, frame rate (30 / 60 FPS), antialiasing, anisotropic filtering (16× by default), graphics API,
  keyboard and gamepad controls, achievements. Works with mouse, keyboard or controller. With Xenos (the emulated GPU)
  ReXGlue renders the game's 1280×720 frame at a whole multiple per axis (2×, 3×) and fits the image to the
  screen; "Automatic" picks the smallest scale that covers your screen (3840×1440 on a 3440×1440 ultrawide). With
  [NativeXRenderer](docs/native-x-renderer-kameo.md) (the default, Vulkan) the internal resolution is your
  screen's or your window's ("Automatic"), or a lower height, with SMAA or FXAA on the scene (the HUD stays sharp).
- [`docs/`](docs/README.md) will have a short note on each problem found and what was done about it.

## Requirements

To build:
- Git, [CMake](https://cmake.org/) 3.25 or newer, [Ninja](https://ninja-build.org/),
  [LLVM/Clang](https://github.com/llvm/llvm-project/releases) (tested with Clang 23; MSVC alone does not build
  this) and the Windows SDK.
- Python 3 (glslang needs it to configure the Vulkan backend).
- Your own copy of the game (see below).

To run:
- Windows 10 or 11, 64-bit, with a GPU that supports Direct3D 12 (or Vulkan).
- A CPU with x86-64-v2 support (SSE4.2 and POPCNT: practically any 64-bit CPU from 2009 on).
- [Microsoft Visual C++ Redistributable 2015–2022 (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe).

## Game files

- **To play:** Kameo Launcher extracts them from the ISO of your disc on its first run (a full Redump image or a
  trimmed one); it checks that it is this game and this version (the USA disc, `default.xex` with CRC32
  `E24BE720`). `"Kameo Launcher.exe" --launcher_first_run` installs them again.
- **To build:** extract the content of your disc or ISO with
  [extract-xiso](https://github.com/XboxDev/extract-xiso/releases) and copy all of it into `assets/` so that
  `assets/default.xex` exists. The expected executable has CRC32 `E24BE720`; `KameoEOPRecomp.exe` warns you at
startup if yours is a different version. See [`assets/README.md`](assets/README.md).

## Building from source

```powershell
git clone --recursive <repository URL> KameoRecomp
cd KameoRecomp
# copy your game files into assets\ first: the build recompiles assets\default.xex
cmake --preset win-amd64-release
cmake --build --preset win-amd64-release
```

- If you cloned without `--recursive`: `git submodule update --init --recursive`.
- `thirdparty/rexglue-sdk` is the SDK with this project's changes (see below).
- The first build translates `default.xex` into C++ (`generated/`) and builds the SDK: it takes a while.
  If it stops with "precompiled header ... has been modified", run the build command again.

## Running

```powershell
.\out\build\win-amd64-release\"Kameo Launcher.exe"   # choose the options, then Play
.\out\build\win-amd64-release\KameoEOPRecomp.exe        # or start the game directly with the saved options
```

- Game files are found in `assets\` next to `KameoEOPRecomp.exe`, otherwise in the repository's `assets\`.
  Use `--game_data_root=<folder>` to point somewhere else.
- Saves, the profile and the shader cache go to `userdata\` next to `KameoEOPRecomp.exe` (if that folder
  cannot be written, for example under Program Files, they go to `Documents\kameo`). Logs go to `logs\`.

### Steam Big Picture

Add `Kameo Launcher.exe` as a non-Steam game: Steam sees the game running until you quit the launcher, and
stopping it from Steam closes the game too. `KameoEOPRecomp.exe` still starts straight into the game with the
options you saved.

## The launcher

**Kameo Launcher** (POTCAWERecomp's launcher, shared through recomp-tools) works with mouse, keyboard or any controller
(DualSense, DualShock, Xbox, Switch Pro...). Its pages:
- **Play:** starts the game, with a summary of the current settings. When you quit the game, the launcher comes
  back.
- **Settings → Video:** monitor, display (fullscreen, borderless or window), window size, image format (fill a
  wide screen, or 16:9 with bars) and internal resolution (NativeXRenderer's, or the Xenos path's scale).
- **Settings → Graphics:** renderer (NativeXRenderer or Xenos), frame rate (30 or 60 FPS), antialiasing, anisotropic
  filtering, shadow quality with NativeXRenderer (Original, High or Ultra: the shadow maps at the internal resolution,
  or at least twice or four times the game's), and the graphics API with Xenos.
- **Settings → Game:** the game's language (text and voices; Spanish of Spain or of Latin America) and the
  cinematics without black bars.
- **Controls → Keyboard:** playing with keyboard and mouse, a primary and a secondary key for each control of
  the Xbox 360 controller, and the mouse for the camera with its sensitivity.
- **Controls → Gamepad:** which button of each type of controller presses each button of the Xbox 360 controller.
- **Achievements:** the game's achievements with their icons, which ones you have unlocked and when, and your
  gamerscore. The game writes their list the first time it runs.
- **Updates:** the installed version (this build does not look for updates).

The first time, if the game files are missing, it asks for the ISO of your copy of the game, checks it and
extracts its files into `assets\` next to `KameoEOPRecomp.exe`. Every change is saved at once in `kameo_options.toml`. With a
controller: LB/RB change the page, A selects, B goes back to the sidebar, Start plays. The launcher starts in
fullscreen; F11 or Alt+Enter switch it to a window. If the game closes with an error, the launcher says so when it
comes back, with a link to the logs.

## Configuration

| File (next to `KameoEOPRecomp.exe`) | What it holds |
|---|---|
| `kameo.toml` | Base settings. Created with the defaults if it is missing; a default setting the file lacks is appended at startup. |
| `kameo_options.toml` | What Kameo Launcher saves (language, monitor, display, window size, renderer, aspect ratio, cinematics, render scale or internal resolution, frame rate, antialiasing, anisotropic filtering, API). Loaded after `kameo.toml`. |

Command-line options (`--name=value`, for example Steam launch options) win over both files:
`--fullscreen=false --window_width=1920 --window_height=1080`,
`--draw_resolution_scale_x=2 --draw_resolution_scale_y=2`, `--gpu_backend=vulkan`, `--kameo_60fps=true`,
`--native_x_renderer=true`, `--kameo_ultrawide=true`, `--kameo_no_letterbox=true`, `--anisotropic_override=0` (off; 5 = 16×), `--user_language=4` (French; see
[`docs/language.md`](docs/language.md)), `--quit_prompt=false` (B on the title screen does not ask to quit).

## How the SDK is modified

`thirdparty/rexglue-sdk` is [ElGuslml/rexglue-sdk](https://github.com/ElGuslml/rexglue-sdk), branch `recomp`: the
official ReXGlue SDK's nightly of 2026-10-02 (v0.10.0 and 24 later commits) with the changes this project, GRAWRecomp
(Ghost Recon Advanced Warfighter), POTCAWERecomp (Pirates of the Caribbean: At World's End) and
[NativeXRenderer](native_x_renderer/README.md) need, one commit each. None of the general fixes is specific to one
game, and the options they add are off by default; the ones for NativeXRenderer change nothing while it is off. The
branch's `RECOMP.md` lists them, and each commit message explains its change. Until 2026-10-21 they were patch series
applied at configure time (`patches/` here and NativeXRenderer's `sdk_patches/`).

A checkout made before that has the old submodule with the patches applied over it; update it with
`git submodule sync` and `git submodule update --init --recursive --force`, then configure again.

NativeXRenderer (`native_x_renderer/`, a submodule: a native renderer that hooks the game's Direct3D calls, developed
on POTCAWERecomp and used by GRAWRecomp) is Kameo's default renderer since 2026-10-21; Kameo Launcher's "Renderer" row
can choose Xenos instead ([`docs/native-x-renderer-kameo.md`](docs/native-x-renderer-kameo.md)).

## Credits

- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk), whose runtime is derived from [Xenia](https://xenia.jp).
- [extract-xiso](https://github.com/XboxDev/extract-xiso), for extracting the game files.
- Margen67 (60 FPS) and ISpectre23 ([FPS Camera Fix](https://github.com/ISpectre23/Kameo-FPS-Camera-Fix)),
  whose [Xenia Canary game patches](https://github.com/xenia-canary/game-patches) for Kameo showed what to
  change for 60 FPS.
- Developed by Gustavo Garcia with the help of Claude (Anthropic).

## License

MIT for this project's own code (see [`LICENSE`](LICENSE)). The ReXGlue SDK and its components keep their own
licenses (see [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)). Kameo: Elements of Power, its code and its
assets belong to their respective owners.
