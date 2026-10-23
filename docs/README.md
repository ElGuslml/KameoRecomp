# Documentation

Short notes on each problem found while making the game run, and what was done about it. Each one gives
the symptom, the cause, the fix and how to turn the fix off, so that you can check it on your own machine.

To build the project and work on it: [Building and developing](development.md).

| Note | Status |
|---|---|
| [Game language](language.md) | Working: launcher language row, Latin American Spanish through a hook |
| [Frame rates: 60 FPS and above](frame-rate.md) | Working: 60 FPS with the camera and the blue bar fixed for it; 120 to 240 FPS (experimental) with the game told 60 Hz |
| [Ultrawide displays](ultrawide.md) | Working: wider 3D view, HUD and videos kept 16:9 in the center (both renderers) |
| [Cinematics without black bars](cinematic-bars.md) | Working: launcher checkbox, at 16:9 and ultrawide |
| [Quitting from the title screen](quit-prompt.md) | Working: B on "PRESS START" asks whether to quit (A quits, B cancels) |
| [CPU usage: one core at 100 %](cpu-usage.md) | Fixed: the game's waits for the GPU and the video reader no longer spin |
| [Overexposed image near the sun](light-glows.md) | Fixed: the light glows' occlusion quads are no longer counted (both renderers) |
| [NativeXRenderer in Kameo](native-x-renderer-kameo.md) | The default renderer since 2026-10-21 (Xenos stays in the launcher's "Renderer" row) |
