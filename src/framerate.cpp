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
// Other code steps by a fixed amount per frame, made for 30 FPS, so it runs twice as fast at 60:
// Kameo's blue bar, drained by an elemental's ability (Pummel Weed's RT + LT), lasted 12 s at 30 FPS
// and 6 s at 60 (Gustavo). Its hook scales the per-frame step by the frame's duration in 30 Hz
// frames, dt * 30, so it keeps the console's speed at any frame rate.
//
// Above 60 FPS the SDK's vblank runs faster (video_mode_refresh_rate, e.g. 120), and the game reads
// that rate from XGetVideoMode: sub_820BECA8 stores it at 0x827556C0, and the clock counts the
// frame time in vblanks, dt * refresh rate (0x82759F7C, read in 408 places: animations, the climb,
// timers), so at 120 Hz they ran twice as fast whatever the frame rate (Gustavo). The game is told
// 60 Hz: dt * 60, the console's unit, at any vblank rate.
//
// The hooks are in kameo_functions.toml; the 60 FPS ones do nothing unless kameo_60fps is on. The
// camera one can be turned off on its own with kameo_camera_fix = false, the per-frame steps with
// kameo_frame_step_fix = false, the refresh rate with kameo_refresh_rate_fix = false (to compare).

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include <atomic>
#include <cstdint>
#include <cstring>

REXCVAR_DEFINE_BOOL(kameo_60fps, false, "Kameo",
                    "Present every vblank (60 FPS) instead of every second one (30 FPS, the "
                    "original), with the camera following fixed for it. The game's clock measures "
                    "real time, so the game speed does not change");
REXCVAR_DEFINE_BOOL(kameo_camera_fix, true, "Kameo",
                    "With kameo_60fps: keep the automatic camera following at the speed it has at "
                    "30 FPS (ISpectre23's FPS Camera Fix). false = the original code, which follows "
                    "at half the speed at 60 FPS");
REXCVAR_DEFINE_BOOL(kameo_frame_step_fix, true, "Kameo",
                    "With kameo_60fps: the game code that steps by a fixed amount per frame (made for 30 FPS) "
                    "keeps the speed it has at 30 FPS: Kameo's blue bar drains as on the console. false = the "
                    "original code, twice as fast at 60 FPS");
REXCVAR_DEFINE_INT32(kameo_frame_rate, 0, "Kameo",
                     "Kameo Launcher's frame rate: 30 (the original), 60, or 120, 144, 160, 165, 240 (the "
                     "vblank at that rate). Sets kameo_60fps and video_mode_refresh_rate at startup; 0 = "
                     "those two as they are");
REXCVAR_DEFINE_BOOL(kameo_refresh_rate_fix, true, "Kameo",
                    "With the vblank above 60 Hz (video_mode_refresh_rate): the game is told 60 Hz, the "
                    "rate its timing counts in, so animations and timers keep their speed. false = the "
                    "SDK's rate, which makes them faster");

namespace {

// The engine's frame time in 30 Hz frames: dt (seconds, the float at 0x82755760, measured by
// sub_820BEBB0 and clamped to 0.1 s) times 30. 1 at 30 FPS, 0.5 at 60.
double Frames30() {
  auto* kernel = rex::system::kernel_state();
  if (!kernel || !kernel->memory()) return 1.0;
  uint32_t bits;
  std::memcpy(&bits, kernel->memory()->virtual_membase() + 0x82755760, 4);
  bits = __builtin_bswap32(bits);
  float dt;
  std::memcpy(&dt, &bits, 4);
  return dt > 0.0f ? double(dt) * 30.0 : 1.0;
}

}  // namespace

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

// [[midasm_hook]] 0x823A0B4C (sub_823A0990, called every frame from sub_821CB988 while an elemental's
// ability drains Kameo's blue bar): after "fsubs f1,f0,f29", f1 = the bar (f0, the stat at +16 of
// Kameo's stats) minus the cost per frame (f29 = 3, the float at 0x8273CF68), which the stat setter
// sub_8217EBA8 stores next. The cost times dt * 30 drains the bar at its 30 FPS speed (90 a second).
void KameoElementalDrainHook(PPCRegister& f0, PPCRegister& f1, PPCRegister& f29) {
  if (!REXCVAR_GET(kameo_60fps) || !REXCVAR_GET(kameo_frame_step_fix)) {
    return;
  }
  f1.f64 = double(float(f0.f64 - f29.f64 * Frames30()));
}

// [[midasm_hook]] 0x820BEDAC (sub_820BECA8, the engine's video setup): after "lfs f1,100(r1)", f1 =
// XGetVideoMode's refresh rate, which the code below stores at 0x827556C0, its inverse at
// 0x827556C4 (the clock's limit is 6 of those) and its whole number at 0x8275D254.
void KameoRefreshRateHook(PPCRegister& f1) {
  if (!REXCVAR_GET(kameo_refresh_rate_fix) || !(f1.f64 > 60.0)) {
    return;
  }
  REXLOG_INFO("[framerate] the game is told 60 Hz instead of {} Hz (kameo_refresh_rate_fix)", f1.f64);
  f1.f64 = 60.0;
}
