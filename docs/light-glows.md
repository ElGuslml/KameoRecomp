# Overexposed, flickering image near the sun (Enchanted Kingdom forest)

**Status: fixed (2026-10-20), on both renderers.**

## Symptom

In the forest of the Enchanted Kingdom, looking towards the sun between the trees, the image is washed out (a white
and yellow haze over half the screen, halos around the trunks) and flickers. It happened with the Xenos path and
with NativeXRenderer, at any internal resolution and frame rate. Turning `occlusion_query_enable` off made it worse.

## Cause

The game draws its light glows by occlusion queries. For each glow it measures one quad twice: a "total" query with
a quad that writes no color, no depth and always passes the depth test, and a "visible" query with the scene's
depth test. The glow's opacity is visible / total times the light's own. The sun's lens flare and the dimming of
the sunlight when the camera faces the sun use the same scheme, with a total quad drawn with the depth test disabled.

Both renderers skip a draw that writes nothing, so the total queries counted 0 and the glows stayed off. On
2026-10-18 both renderers were changed to count those quads (NativeXRenderer decision D79, SDK patch 0026), which
turned the glows on: around the sun they produced the haze, and they flickered as the foliage moved. Without them
the forest looks as it should. Xenia Canary's notes also name this game among those that look more correct with
those counts at zero.

## Fix

The 2026-10-18 change is undone: SDK patch 0026 is removed from the SDK changes (then `patches/`) and NativeXRenderer
decision D87 withdraws D79. The quads are not counted, the glows and the sun flare stay off, as before that change.

Nothing to turn on or off. `occlusion_query_enable = false` (the SDK's fake "everything visible" answer) turns all
of them on and brings the haze back.
