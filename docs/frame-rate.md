# 60 FPS option (and above)

**Status: working (2026-10-12; 120 FPS since 2026-10-23).** 60 FPS measured in gameplay and in a cinematic; the
game speed is unchanged (same route with the same timed inputs, cinematic at the same pace). 120 FPS (the vblank at
120 Hz) tested by Gustavo in Thorn's Castle: Chilla's climb and Major Ruin at their 60 Hz speed.

## Symptom

The game runs at 30 FPS: one new image every two refreshes of the 60 Hz display.

## Cause

The cap is the game's Direct3D present interval, not its speed. The renderer leaves
`D3DRS_PRESENTINTERVAL` at "every second vblank" (`D3DPRESENT_INTERVAL_TWO`), and the Direct3D library
linked into the game turns that into the swap descriptor on every frame, so the image is flipped only every
second vblank.

The engine measures the real time of each frame (time base counter and its frequency, clamped to 0.1 s) and
scales its updates by it, so it runs at the same speed at 60 FPS. One known exception, found by ISpectre23
for Xenia: the automatic camera following. In X and Z the camera's target is its position plus a velocity
times the frame time, and the camera then moves towards it by a factor that is again proportional to the
frame time, so the movement per second halves at 60 FPS: the camera lags behind Kameo.

Other code steps by a fixed amount every frame, sized for 30 FPS, so it runs twice as fast at 60. Found so far:
Kameo's blue bar, drained by an elemental's ability (Pummel Weed's RT + LT), lasted 12 s at 30 FPS and 6 s at 60.

Above 60 FPS the emulated vblank runs faster (`video_mode_refresh_rate`, for example 120), and the game reads that
rate from the video mode at startup. Its clock counts the frame time in vblanks (frame time × refresh rate), and
animations, Chilla's climb and many timers advance by that count, so at 120 Hz they ran twice as fast, at 120 or at
60 FPS alike.

## Fix

`src/framerate.cpp` with four `[[midasm_hook]]`s in `kameo_functions.toml`, the first three active only with
`kameo_60fps`:

- In the Direct3D swap routine, right after the present interval is read, 2 becomes 1. This is what
  Margen67's "60 FPS" Xenia patch does (`li r10,2` → `li r10,1` at `0x820CF93C`).
- In the camera following, the X and Z distances to the target are divided by the frame time × 30, which
  gives the follow factor `v / 30` of ISpectre23's "FPS Camera Fix" (Xenia patch at `0x82440F18`–
  `0x82440FB8`). At exactly 30 FPS both are the original values; Y (height) is unchanged, as in the patch.
- In the ability that drains the blue bar (`sub_823A0990`, every frame), the cost per frame (3) is multiplied by
  the frame time × 30, so the bar drains at its 30 FPS speed (90 per second) at any frame rate.
- In the engine's video setup (`sub_820BECA8`), a refresh rate above 60 Hz is reported to the game as 60 Hz, the
  console's, so the clock counts 60 Hz vblanks at any vblank rate.

## 120 FPS

No launcher row yet: start the game with `--video_mode_refresh_rate=120` (with **Frame rate: 60 FPS** in the
launcher). The display should run at 120 Hz or more.

## Turning it off

Choose **Frame rate: 30 FPS** in Kameo Launcher (it writes `kameo_60fps = false` to `kameo_options.toml`)
or start the game with `--kameo_60fps=false`. To see the camera without its fix at 60 FPS:
`--kameo_camera_fix=false`; the per-frame steps without their fix: `--kameo_frame_step_fix=false`; the
SDK's refresh rate given to the game as it is: `--kameo_refresh_rate_fix=false`.

## What to watch for

Anything that runs twice as fast or drifts at 60 FPS (an animation, a cinematic, a puzzle, physics) and
camera behavior: the camera should follow Kameo as it does at 30 FPS. Note where it happened and switch back
to 30 FPS in the launcher.

| Option (`kameo_options.toml`, `kameo.toml` or `--name=value`) | Default | Meaning |
|---|---|---|
| `kameo_60fps` | `false` | `true` presents every vblank (60 FPS); `false` keeps the original 30 FPS |
| `kameo_camera_fix` | `true` | With `kameo_60fps`: camera following at its 30 FPS speed; `false` = original code |
| `kameo_frame_step_fix` | `true` | With `kameo_60fps`: code stepped per frame (the blue bar) at its 30 FPS speed; `false` = original code |
| `kameo_refresh_rate_fix` | `true` | With the vblank above 60 Hz: the game is told 60 Hz; `false` = the SDK's rate (animations and timers too fast) |
| `video_mode_refresh_rate` | `60` | The emulated vblank (SDK); `120` with `kameo_60fps` gives 120 FPS |
