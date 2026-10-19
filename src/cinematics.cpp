// kameo - cinematics without the black bars.
//
// The in-engine cinematics are letterboxed by sub_822712E0: two black rectangles of 1/8 of the screen's height,
// drawn over the full 1280x720 scene through sub_822D19E0. With kameo_no_letterbox the two calls are skipped
// ([[midasm_hook]]s with jump_address_on_true in kameo_functions.toml); the rest of the function, which animates
// the bars' level, runs as before. Works at any aspect ratio (with kameo_ultrawide, src/widescreen.cpp).

#include <rex/cvar.h>
#include <rex/logging.h>

#include <atomic>

REXCVAR_DEFINE_BOOL(kameo_no_letterbox, false, "Kameo",
                    "Cinematics: do not draw the black bars above and below the image (false = the original)");

// [[midasm_hook]] at the two "bl 0x822d19e0" of sub_822712E0 (the top and the bottom bar): true skips the call.
bool KameoLetterboxBarHook() {
  if (!REXCVAR_GET(kameo_no_letterbox)) return false;
  static std::atomic<bool> logged{false};
  if (!logged.exchange(true)) {
    REXLOG_INFO("[cinematics] letterbox bars not drawn (kameo_no_letterbox)");
  }
  return true;
}
