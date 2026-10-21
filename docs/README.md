# Documentation

Short notes on each problem found while making the game run, and what was done about it. Each one gives
the symptom, the cause, the fix and how to turn the fix off, so that you can check it on your own machine.

| Note | Status |
|---|---|
| [Game language](language.md) | Working: launcher language row, Latin American Spanish through a hook |
| [60 FPS option](frame-rate.md) | Working, being tested: present interval 2 → 1, camera following fixed for it |
| [Ultrawide displays](ultrawide.md) | Working: wider 3D view, HUD and videos kept 16:9 in the center (both renderers) |
| [Cinematics without black bars](cinematic-bars.md) | Working: launcher checkbox, at 16:9 and ultrawide |
| [CPU usage: one core at 100 %](cpu-usage.md) | Fixed: the game's waits for the GPU and the video reader no longer spin |
| [Overexposed image near the sun](light-glows.md) | Fixed: the light glows' occlusion quads are no longer counted (both renderers) |
| [NativeXRenderer in Kameo](native-x-renderer-kameo.md) | The default renderer since 2026-10-21 (Xenos stays in the launcher's "Renderer" row) |
