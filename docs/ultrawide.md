# Ultrawide displays

**Status: working (2026-10-19).** Tested on the title screen, the main menu, the first level's opening video, the
start of Thorn's Castle (HUD, help box, pause menu) in a 2560×1080 window, with both renderers, then in a long play
session on a 3440×1440 screen without problems.

## Symptom

On a screen wider than 16:9 (21:9, 32:9) the game shows its 16:9 image with black bars at the sides. Stretching
the image over the whole screen (`present_letterbox = false`) distorts everything horizontally.

## Cause

The game renders a 1280×720 frame and builds every camera for a 16:9 aspect ratio (a 16:9 constant at
`0x82087CAC`, or 1280 / 720). Its 2D layer (HUD, menus, pause menu, text) is laid out on a 640×480 or 1280×720
canvas that always maps to the whole frame, and its videos fill the frame.

## Fix

`src/widescreen.cpp` with `[[midasm_hook]]`s in `kameo_functions.toml`, active only with `kameo_ultrawide`
(Kameo Launcher: **Aspect ratio: Ultrawide (full width)**, on a screen or window wider than 16:9). The 1280×720
frame is stretched over the screen (anamorphic), and the game is corrected by the same factor
(screen aspect ratio ÷ 16:9, for example 1.34 on 3440×1440):

- **3D views see more to the sides (Hor+).** The perspective projection (`sub_820DFD38`) and the camera's
  culling frustum (`sub_820ED110`) of every 16:9 view get the wider aspect ratio, so nothing at the sides is
  culled.
- **The 2D layer stays 16:9 in the center** (`kameo_ultrawide_hud`). The two builders of its orthographic
  projections (`sub_820DFE20` for text and menus, `D3DXMatrixOrthoOffCenterLH` for the HUD's sprites) get a
  canvas wider by the factor when they cover the whole screen and the game draws into the screen; the menu
  book's pages, drawn into a 640×480 texture, are left alone. A colored rectangle across the whole width (a
  fade, the cinematics' black bars; `sub_8224B118`, `sub_822D19E0`) keeps spanning it. The bars can be removed
  altogether ([cinematic-bars.md](cinematic-bars.md)).
- **Videos stay 16:9 in the center** with black sides (`sub_82265558`, the Bink frame).

On the Xenos path the presenter stretches the image (`present_letterbox = false`, set automatically with
`kameo_ultrawide`) and the render scale is chosen per axis (3440×1440 → 3×2 = 3840×1440). NativeXRenderer
renders at the screen's aspect ratio instead (`native_x_renderer_fill_screen`), and the same hooks apply.

This is what Margen67's "Aspect Ratio" Xenia patch for this game starts from (the 16:9 constant changed to
21.5:9); that patch alone stretches the HUD and the videos.

## Turning it off

Choose **Aspect ratio: 16:9 (original)** in Kameo Launcher, or start the game with `--kameo_ultrawide=false`.
To keep the wider view but stretch the HUD and videos: `--kameo_ultrawide_hud=false`.

| Option (`kameo_options.toml`, `kameo.toml` or `--name=value`) | Default | Meaning |
|---|---|---|
| `kameo_ultrawide` | `false` | `true`: the cameras use the window's aspect ratio and the image fills a wider screen (Xenos) |
| `kameo_ultrawide_hud` | `true` | With ultrawide: HUD, menus and videos kept 16:9 in the center; `false` = stretched |
| `native_x_renderer_fill_screen` | `false` | NativeXRenderer renders at the screen's aspect ratio (the launcher sets it with ultrawide) |

## What to watch for

A 2D element that is stretched or off-center, or one that should cover the screen and covers only its middle
(a fade, a flash, a full-screen overlay); objects that pop in or out at the sides of the screen; a cinematic
whose framing looks wrong. Note where it happened and switch back to 16:9 in the launcher.
