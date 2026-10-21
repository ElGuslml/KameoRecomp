// kameo - quitting from the title screen.
//
// Kameo has no way out of itself (on the console the dashboard was). On the title screen ("PRESS START"), B now
// asks whether to quit the game: the SDK's quit prompt (rex/ui/overlay/quit_prompt.h, on the recomp branch), a
// dialog in the game's language where A quits (the window closes, as with its close button) and B cancels. The game
// does not see those presses.
//
// sub_82414408 runs the title sequence every frame; its state is a byte at [r29+5]: 8 is the title screen with
// "PRESS START" (the state set together with that text, string 39), 9-15 the main menu over it, 16-17 the attract
// video. The hook offers the prompt while the state is 8, so a B pressed in the main menu still goes back to the
// title screen, and only a new press there opens the dialog. quit_prompt = false (an SDK cvar) disables it.

#include <rex/ppc/context.h>
#include <rex/ui/overlay/quit_prompt.h>

namespace {

constexpr uint32_t kTitleScreenState = 8;

}  // namespace

// [[midasm_hook]] at 0x8241460C (cmpwi cr6,r10,8 in sub_82414408): r10 = the title sequence's state.
void KameoTitleStateHook(PPCRegister& r10) {
  if (r10.u32 == kTitleScreenState) {
    rex::ui::OfferQuitPrompt();
  }
}
