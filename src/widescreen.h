// kameo - ultrawide displays.
//
// Anamorphic rendering: the game's cameras see with the window's aspect ratio (e.g. 3440x1440) but still
// draw into its 1280x720 image (x render scale); the SDK presenter stretches it over the whole window
// (present_letterbox = false). Enabled with the kameo_ultrawide cvar.
// With NativeXRenderer the renderer stretches the image instead: it renders at the output's aspect ratio when
// native_x_renderer_fill_screen is on (native_x_renderer/docs/decisions.md, D43), and the same hooks widen the
// cameras and keep the HUD at 16:9 by its native_x_renderer::AspectFactor().

#pragma once

#include <cstdint>

namespace kameo {

// Size of the game window in pixels (from ReXApp::OnWindowPixelSizeChanged). With NativeXRenderer drawing,
// the factor is the renderer's, fixed when it started (call this after native_x_renderer::Initialize).
void SetOutputSize(uint32_t width, uint32_t height);

}  // namespace kameo
