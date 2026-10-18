// kameo - 60 FPS: present every vblank instead of every second one, and keep the automatic camera
// following at the same speed.
//
// The game runs at 30 FPS only because of its Direct3D present interval: its renderer leaves
// D3DRS_PRESENTINTERVAL at D3DPRESENT_INTERVAL_TWO, and the XDK Direct3D library linked into the
// game turns that into the swap descriptor on every Swap (sub_820CF868: 0/1 -> 1, 2 -> 2, 4 -> 3),
// so the vblank callback flips the front buffer only every second vblank of the SDK's 60 Hz. The
// engine's clock is frame-rate independent: sub_820BEBB0 measures the real time per frame (time
// base / performance frequency) into the global at 0x82755760, clamped to 0.1 s, and the game code
// scales by it (usually as dt * 30, the global at 0x8273E094, "30 Hz frames elapsed").
//
// One known exception, found by ISpectre23 for Xenia ("FPS Camera Fix" in the Xenia Canary game
// patches): the automatic camera following of sub_82440D28. Its target position is the current
// position plus a velocity times dt, and the camera then moves towards it by a factor v * dt (v,
// the follow strength at camera+104), so in X and Z the camera moves by dt squared per frame: at
// 60 FPS it follows at half the speed. ISpectre23's patch uses v / 30 instead of v * dt for X and
// Z (the same value at 30 FPS); Y (the height, not dt-scaled) keeps v * dt.
//
// Both hooks are in kameo_functions.toml and do nothing unless kameo_60fps is on; the camera one
// can be turned off on its own with kameo_camera_fix = false (to compare).

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>

#include <atomic>
#include <cstdint>

REXCVAR_DEFINE_BOOL(kameo_60fps, false, "Kameo",
                    "Present every vblank (60 FPS) instead of every second one (30 FPS, the "
                    "original), with the camera following fixed for it. The game's clock measures "
                    "real time, so the game speed does not change");
REXCVAR_DEFINE_BOOL(kameo_camera_fix, true, "Kameo",
                    "With kameo_60fps: keep the automatic camera following at the speed it has at "
                    "30 FPS (ISpectre23's FPS Camera Fix). false = the original code, which follows "
                    "at half the speed at 60 FPS");

// [[midasm_hook]] 0x820CF918 (sub_820CF868, Direct3D Swap): before "cmplwi cr6,r10,1", r10 = the
// D3DRS_PRESENTINTERVAL render state just loaded from the device (0 = default, 1 = one, 2 = two,
// 4 = three, 0x80000000 = immediate). The same change as Margen67's "60 FPS" Xenia patch (li r10,2
// -> li r10,1 at 0x820CF93C).
void KameoPresentIntervalHook(PPCRegister& r10) {
  if (!REXCVAR_GET(kameo_60fps) || r10.u32 != 2) {
    return;
  }
  static std::atomic<bool> logged{false};
  if (!logged.exchange(true)) {
    REXLOG_INFO("[framerate] present interval 2 -> 1 (kameo_60fps)");
  }
  r10.u32 = 1;
}

// [[midasm_hook]] 0x82440FA4 (sub_82440D28, camera following): before the three fmadds that write
// the new camera position, current + (target - current) * f0, with f0 = v * dt and f8, f7, f6 the
// X, Y, Z differences target - current. f31 = dt * 30 here (set at the top of the function and not
// changed since). Dividing the X and Z differences by f31 makes their factor v / 30, as in
// ISpectre23's patch. f31 is never 0 in play; without time there is nothing to follow anyway
// (target - current is proportional to dt).
void KameoCameraFollowHook(PPCRegister& f8, PPCRegister& f6, PPCRegister& f31) {
  if (!REXCVAR_GET(kameo_60fps) || !REXCVAR_GET(kameo_camera_fix) || !(f31.f64 > 0.0)) {
    return;
  }
  f8.f64 = double(float(f8.f64 / f31.f64));
  f6.f64 = double(float(f6.f64 / f31.f64));
}
