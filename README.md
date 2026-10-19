# KameoRecomp

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
- **Game language** chosen in the launcher: English, French, German, Italian, Spanish (Spain or Latin
  America), Japanese, Korean or Traditional Chinese ([`docs/language.md`](docs/language.md)).
- **Kameo Launcher**: language, monitor, fullscreen / borderless / window, window resolution, renderer,
  aspect ratio (16:9 / ultrawide), internal resolution, frame rate (30 / 60 FPS), antialiasing, anisotropic
  filtering (16× by default) and graphics API. Works
  with mouse, keyboard or controller; "Save" keeps the window open. With Xenos (the emulated GPU, the default)
  ReXGlue renders the game's 1280×720 frame at a whole multiple per axis (2×, 3×) and fits the image to the
  screen; "Automatic" picks the smallest scale that covers your screen (3840×1440 on a 3440×1440 ultrawide). With
  [NativeXRenderer](docs/native-x-renderer-kameo.md) (experimental, Vulkan) the internal resolution is your
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

Extract the content of your disc or ISO with
[extract-xiso](https://github.com/XboxDev/extract-xiso/releases) and copy all of it into `assets/` so that
`assets/default.xex` exists. The expected executable has CRC32 `E24BE720`; `kameorecomp.exe` warns you at
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
- Configuring applies the SDK fixes in [`patches/`](patches/) to `thirdparty/rexglue-sdk` (see below).
- The first build translates `default.xex` into C++ (`generated/`) and builds the SDK: it takes a while.
  If it stops with "precompiled header ... has been modified", run the build command again.

## Running

```powershell
.\out\build\win-amd64-release\"Kameo Launcher.exe"   # choose the options, then Play
.\out\build\win-amd64-release\kameorecomp.exe        # or start the game directly with the saved options
```

- Game files are found in `assets\` next to `kameorecomp.exe`, otherwise in the repository's `assets\`.
  Use `--game_data_root=<folder>` to point somewhere else.
- Saves, the profile and the shader cache go to `userdata\` next to `kameorecomp.exe` (if that folder
  cannot be written, for example under Program Files, they go to `Documents\kameo`). Logs go to `logs\`.

### Steam Big Picture

Add `kameorecomp.exe` as a non-Steam game: it starts straight into the game with the options you saved in
Kameo Launcher. You can also add `Kameo Launcher.exe` to change options from the couch with a controller.

## Configuration

| File (next to `kameorecomp.exe`) | What it holds |
|---|---|
| `kameo.toml` | Base settings. Created with the defaults if it is missing; a default setting the file lacks is appended at startup. |
| `kameo_options.toml` | What Kameo Launcher saves (language, monitor, display, window size, renderer, aspect ratio, render scale or internal resolution, frame rate, antialiasing, anisotropic filtering, API). Loaded after `kameo.toml`. |

Command-line options (`--name=value`, for example Steam launch options) win over both files:
`--fullscreen=false --window_width=1920 --window_height=1080`,
`--draw_resolution_scale_x=2 --draw_resolution_scale_y=2`, `--gpu_backend=vulkan`, `--kameo_60fps=true`,
`--native_x_renderer=true`, `--kameo_ultrawide=true`, `--anisotropic_override=0` (off; 5 = 16×), `--user_language=4` (French; see
[`docs/language.md`](docs/language.md)).

## How the SDK is modified

`thirdparty/rexglue-sdk` is the official ReXGlue SDK at v0.10.0. The fixes are in [`patches/`](patches/), a
`git format-patch` series applied at configure time by
[`cmake/apply_sdk_patches.cmake`](cmake/apply_sdk_patches.cmake). They come from GRAWRecomp (Ghost Recon
Advanced Warfighter) and POTCAWERecomp (Pirates of the Caribbean: At World's End), where they were found;
none of them is specific to those games, and the options they add are off by default. Each patch's commit
message explains the change:

| Patch | Change |
|---|---|
| 0001 | Keep host FP exceptions masked when the game updates the FPSCR |
| 0002 | Invalidate GPU copies of newly allocated physical memory |
| 0003 | Code generator: `vpkd3d128` lost the sign when the destination aliases the source |
| 0004 | `gpu_depth_clamp`: clamp depth instead of clipping in Z, like Xenos (D3D12 and Vulkan) |
| 0005 | `fullscreen_borderless_window` option |
| 0006 | `gpu_backend` option to choose D3D12 or Vulkan |
| 0007 | `XGetLanguage` honors `user_language` |
| 0008 | No busy waiting when there is no work (timer queue, presenter repaints) |
| 0009 | `gpu_async_pipelines_no_frame_wait`: do not stall the frame on new pipelines |
| 0010–0013 | Faster register and shader-constant writes from the command processor |
| 0014 | Log the guest context on unhandled guest access violations |
| 0015 | `gpu_guest_fps_limit`: optional frame rate cap for the game |
| 0016 | `XamUserReadProfileSettings` no longer writes past the game's buffer (title-specific settings larger than their nominal size) |
| 0017 | `render_target_skip_transfers_on_overwrite`: no EDRAM ownership transfer into a render target that a single triangle, rectangle or quad fully overwrites |
| 0018 | `render_target_draw_clears`: a draw that writes constant values to a rectangle of its render targets is done as a clear of the render targets owning that EDRAM, never through the alias |
| 0019 | `d3d12_lazy_resolve`: color resolves kept as host render target copies and textures filled from them by copying, the shared memory written only on demand (no gain measured so far) |
| 0020 | Occlusion queries: the end pairs with the begin whatever its address and in every EDRAM tile, so host queries are no longer turned off at the first query (D3D12 and Vulkan) |
| 0021 | SPIR-V: cube maps sampled at a LOD computed in the shader, not with gradients (NVIDIA's Vulkan driver returned garbage in one pixel of each quad), and the LOD bias scale kept finite |
| 0022 | Code generator: with `non_volatile_as_local` the elided `__savegprlr_N` still stores the return address in the frame's LR slot, which the guest's stack walks read |
| 0023 | `WAIT_REG_MEM` polls every 100 µs with the high-resolution timer instead of sleeping 1 ms |
| 0024 | CMake: rexruntime and the GPU plugins are copied next to the executable on every build, not only when it links again |
| 0025 | Code generator: a `share_registers` function (an SEH funclet, a setjmp caller) keeps its registers across calls when the others are local variables |
| 0026 | Occlusion queries: inside a guest query, a draw whose depth test always passes without writing gets a stencil test that always passes and keeps, so it rasterizes and its samples are counted (this game's light glows, measured by such a quad, never showed) (D3D12 and Vulkan) |

[NativeXRenderer](native_x_renderer/README.md) (`native_x_renderer/`, a submodule: a native renderer that hooks the
game's Direct3D calls, developed on POTCAWERecomp and used by GRAWRecomp) brings its own SDK patches
(`native_x_renderer/sdk_patches/`), applied after this series. They change nothing while NativeXRenderer is off.
In Kameo it is experimental: Kameo Launcher's "Renderer" row chooses it, Xenos stays the default
([`docs/native-x-renderer-kameo.md`](docs/native-x-renderer-kameo.md)).

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
