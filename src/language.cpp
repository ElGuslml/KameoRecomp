// kameo - game language: Latin American Spanish.
//
// The game picks its language once at start-up (sub_820B4F08): XGetLanguage() (the SDK's
// user_language: 1 English, 2 Japanese, 3 German, 4 French, 5 Spanish, 6 Italian, 7 Korean,
// 8 Traditional Chinese) and, for Spanish on a North American console (XGetGameRegion() == 0xFF),
// its own value 10, the Latin American Spanish of the "Mexican" folder. The result goes to the
// global at 0x827556B4, from which sub_820BF430 builds the language folder (game:\<language>\,
// English for anything else). The SDK's XGetGameRegion returns 0xFFFF (all regions), so the game
// always takes the Spanish of Spain; the hook below gives it the North American region when
// kameo_latin_american_spanish is on, and the game's own code does the rest.

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>

#include <cstdint>

REXCVAR_DEFINE_BOOL(kameo_latin_american_spanish, false, "Kameo",
                    "With user_language = 5 (Spanish): Latin American Spanish (the game's "
                    "\"Mexican\" folder, what it uses on a North American console) instead of the "
                    "Spanish of Spain");

// [[midasm_hook]] 0x820B5230 (sub_820B4F08): before "cmplwi cr6,r31,5", r31 = XGetLanguage() and
// r3 = XGetGameRegion().
void KameoLanguageHook(PPCRegister& r31, PPCRegister& r3) {
  bool latin_american = r31.u32 == 5 && REXCVAR_GET(kameo_latin_american_spanish);
  if (latin_american) {
    r3.u32 = 0xFF;  // XC_GAME_REGION_NA_ALL
  }
  REXLOG_INFO("[language] XGetLanguage = {}{}", r31.u32,
              latin_american ? " (Latin American Spanish)" : "");
}
