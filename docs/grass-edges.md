# Dark edges and see-through grass (NativeXRenderer)

**Status: fixed (2026-11-11), NativeXRenderer only.**

## Symptom

With NativeXRenderer, the blades of the tall grass in the Enchanted Kingdom had dark outlines, and the grass behind
them showed through less. With the Xenos path they did not.

## Cause

The game draws the grass with alpha to coverage: the Xbox 360 turns each pixel's alpha into how many of its samples
are covered. The game draws without MSAA, so on the console a pixel is either drawn opaque or discarded, by a 2x2
pattern that follows the alpha, and the grass drawn behind it afterwards fills the discarded pixels. The Xenos path
does the same.

NativeXRenderer meant to keep that 2x2 pattern on surfaces without MSAA (its decision D114). But the shader
translator writes the pattern through the shader's sample mask, which it only does when the GPU's sample rate shading
is enabled, and NativeXRenderer did not enable it: alpha to coverage was simply off. Every pixel that passed the
game's own alpha test was drawn and wrote depth, including the nearly transparent, dark pixels at the edges of the
blades.

Blending by the alpha instead (`native_x_renderer_alpha_to_coverage = "blend"`, NativeXRenderer's D69) removed the
outlines but made the grass see-through: the ground drawn before it showed through the blades.

## Fix

NativeXRenderer's decision D138: its Vulkan device enables sample rate shading where the GPU has it (its DiligentCore
patch 0012), and the shader translator uses it. Alpha to coverage on surfaces without MSAA is then the console's 2x2
pattern, as on the Xenos path: no dark outlines, and the grass is solid. At the screen's own resolution the blades'
edges show the pattern as a fine stipple; the Xenos path at a higher internal resolution smooths it when it scales
the image down.

Nothing to turn off. The log says `sample rate shading on: alpha to coverage on 1x targets dithers in 2x2 as on the
console (D138)`; on a GPU without sample rate shading it says `off`, and the outlines come back there.
