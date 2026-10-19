// kameo - ultrawide displays (see widescreen.h).

#include "widescreen.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>
#include <native_x_renderer/native_x_renderer.h>

#include <atomic>
#include <cstring>
#include <utility>

REXCVAR_DEFINE_BOOL(kameo_ultrawide, false, "Kameo",
                    "Ultrawide: the game's cameras use the window's aspect ratio (anamorphic, the presenter "
                    "stretches the 16:9 image; needs present_letterbox = false). With NativeXRenderer, "
                    "native_x_renderer_fill_screen does this instead");
REXCVAR_DEFINE_BOOL(kameo_ultrawide_hud, true, "Kameo",
                    "With kameo_ultrawide: keep the HUD, the menus' 2D layer and the videos at 16:9 in the "
                    "center (false = stretched)");

namespace kameo {
namespace {

constexpr double kGuestAspect = 16.0 / 9.0;

// Factor applied to the aspect ratio of 16:9 views (1 = unchanged).
std::atomic<double> g_aspect_factor{1.0};

}  // namespace

void SetOutputSize(uint32_t width, uint32_t height) {
  double factor = 1.0;
  if (native_x_renderer::Renders()) {
    // NativeXRenderer renders at the output's aspect ratio when it fills it (the profile says these hooks
    // widen the cameras, D43): the hooks follow its factor, whatever kameo_ultrawide says. They handle wider
    // outputs only (the 2D layer is narrowed around its center): Kameo Launcher turns filling on for those only.
    double native = native_x_renderer::AspectFactor();
    if (native > 1.0) {
      factor = native;
    } else if (native < 1.0) {
      static std::atomic<bool> warned{false};
      if (!warned.exchange(true)) {
        REXLOG_WARN("[ultrawide] NativeXRenderer fills an output taller than 16:9 ({:.4f}); the game's hooks only "
                    "widen views, so the image is stretched vertically (native_x_renderer_fill_screen = false "
                    "keeps 16:9 with bars)",
                    native);
      }
    }
  } else if (REXCVAR_GET(kameo_ultrawide) && width > 0 && height > 0) {
    double aspect = double(width) / double(height);
    if (aspect > kGuestAspect) {
      factor = aspect / kGuestAspect;
    }
  }
  if (g_aspect_factor.exchange(factor) != factor) {
    REXLOG_INFO("[ultrawide] output {}x{}: aspect factor {:.4f}", width, height, factor);
  }
}

}  // namespace kameo

namespace {

// 16:9 views: an aspect ratio between these (the cameras pass 1280 / 720 or the 16:9 constant at 0x82087CAC).
constexpr double kMinAspect = 1.70, kMaxAspect = 1.86;

double WidenFactor() { return kameo::g_aspect_factor.load(std::memory_order_relaxed); }

// The factor the 2D layer is narrowed by (1 = stretched with the image).
double NarrowFactor() { return REXCVAR_GET(kameo_ultrawide_hud) ? WidenFactor() : 1.0; }

void LogOnce(std::atomic<bool>& logged, const char* what, double factor) {
  if (!logged.exchange(true)) {
    REXLOG_INFO("[ultrawide] {} ({:.4f})", what, factor);
  }
}

uint32_t Load32(uint32_t address) {
  auto* kernel = rex::system::kernel_state();
  if (!kernel || !kernel->memory()) return 0;
  uint32_t value;
  std::memcpy(&value, kernel->memory()->virtual_membase() + address, 4);
  return __builtin_bswap32(value);
}

// Whether the game draws into the screen: the Direct3D device's render target 0 (the device pointer at
// 0x82766ABC, its render targets at +12536, as in src/native_x_renderer_kameo.cpp) is 1280x720 (the surface's
// size at +24 as a 2D texture's fetch dword 2: width - 1 in bits 0-12, height - 1 in bits 13-25). The menu
// draws its book's pages into a 640x480 target with the same 2D projections; those stay as they are.
bool DrawsIntoScreen() {
  uint32_t device = Load32(0x82766ABC);
  uint32_t surface = device ? Load32(device + 12536) : 0;
  if (!surface) return false;
  uint32_t size = Load32(surface + 24);
  return (size & 0x1FFF) + 1 == 1280 && ((size >> 13) & 0x1FFF) + 1 == 720;
}

// A 2D projection of the whole screen: the game lays its 2D layer out in 640x480 or 1280x720 units, top left
// at (0, 0).
bool IsScreenCanvas(double left, double right, double top, double bottom) {
  return left == 0.0 && top == 0.0 &&
         ((right == 640.0 && bottom == 480.0) || (right == 1280.0 && bottom == 720.0));
}

// Narrows [left, right] around its center by the factor: what is drawn with the projection keeps its
// proportions in the middle of the stretched image.
void Narrow(PPCRegister& left, PPCRegister& right, double factor) {
  double center = (left.f64 + right.f64) / 2;
  left.f64 = center - (center - left.f64) * factor;
  right.f64 = center + (right.f64 - center) * factor;
}

// Set by sub_8224B118 when it fills the whole screen (a fade): its projection, the next one built on this
// thread, is not narrowed.
thread_local bool g_full_screen_fill = false;

}  // namespace

// [[midasm_hook]] at the entry of sub_820DFD38 (kameo_functions.toml), the game's perspective projection
// (D3DXMatrixPerspectiveFovLH style): r3 = the matrix, f1 = vertical fov, f2 = aspect ratio, f3/f4 = near/far.
// Only 16:9 views are widened.
void KameoProjectionAspectHook(PPCRegister& f2) {
  double factor = WidenFactor();
  if (factor == 1.0 || f2.f64 < kMinAspect || f2.f64 > kMaxAspect) return;
  static std::atomic<bool> logged{false};
  LogOnce(logged, "3D views widened", factor);
  f2.f64 *= factor;
}

// [[midasm_hook]] in sub_820ED110, which builds a camera's culling frustum from its fov, aspect ratio and far
// plane (+0, +4, +12 of r3; sub_820ED248 calls it after the projection): f12 = the aspect ratio just loaded.
// Widened like the projection, so that nothing at the sides of the wider view is culled.
void KameoFrustumAspectHook(PPCRegister& f12) {
  double factor = WidenFactor();
  if (factor == 1.0 || f12.f64 < kMinAspect || f12.f64 > kMaxAspect) return;
  f12.f64 *= factor;
}

// [[midasm_hook]] at the entry of sub_820DFE20, the game's 2D projection (an off-center orthographic
// matrix): f1 = top, f2 = bottom, f3 = left, f4 = right, f5/f6 = near/far. The HUD's text and the menus'
// 2D layer use it.
void KameoOrthoAspectHook(PPCRegister& f1, PPCRegister& f2, PPCRegister& f3, PPCRegister& f4) {
  bool full_screen_fill = std::exchange(g_full_screen_fill, false);
  double factor = NarrowFactor();
  if (factor == 1.0 || full_screen_fill || !IsScreenCanvas(f3.f64, f4.f64, f1.f64, f2.f64) ||
      !DrawsIntoScreen()) {
    return;
  }
  static std::atomic<bool> logged{false};
  LogOnce(logged, "2D layer kept at 16:9 in the center", factor);
  Narrow(f3, f4, factor);
}

// [[midasm_hook]] at the entry of sub_8251CFF0, D3DXMatrixOrthoOffCenterLH: f1 = left, f2 = right,
// f3 = bottom, f4 = top, f5/f6 = near/far. The HUD's sprites (health, the elements, the button prompts) use it.
void KameoD3DXOrthoAspectHook(PPCRegister& f1, PPCRegister& f2, PPCRegister& f3, PPCRegister& f4) {
  double factor = NarrowFactor();
  if (factor == 1.0 || !IsScreenCanvas(f1.f64, f2.f64, f4.f64, f3.f64) || !DrawsIntoScreen()) return;
  Narrow(f1, f2, factor);
}

// [[midasm_hook]] at the entry of sub_8224B118, which fills a rectangle with a color through its own 2D
// projection (sub_820DFE20): f1 = top, f2 = left, f3 = height, f4 = width, in 640x480 units when r9 != 0,
// otherwise in the screen's (1280x720). A rectangle over the whole screen (a fade) keeps covering it.
void KameoFillAspectHook(PPCRegister& f1, PPCRegister& f2, PPCRegister& f3, PPCRegister& f4, PPCRegister& r9) {
  double width = r9.u32 ? 640.0 : 1280.0, height = r9.u32 ? 480.0 : 720.0;
  g_full_screen_fill = f1.f64 <= 0.0 && f2.f64 <= 0.0 && f1.f64 + f3.f64 >= height && f2.f64 + f4.f64 >= width;
}

// [[midasm_hook]] at the entry of sub_82265558, which draws a video frame (Bink) as a rectangle in 1280x720
// units: f1/f2 = left/top, f3/f4 = scale, r5/r6 = the frame's width/height. Kept at 16:9 in the center:
// x' = 640 + (x - 640) / k (the sides stay black).
void KameoVideoAspectHook(PPCRegister& f1, PPCRegister& f3) {
  double factor = NarrowFactor();
  if (factor == 1.0 || !DrawsIntoScreen()) return;
  static std::atomic<bool> logged{false};
  LogOnce(logged, "videos kept at 16:9 in the center", factor);
  f1.f64 = 640.0 + (f1.f64 - 640.0) / factor;
  f3.f64 /= factor;
}
