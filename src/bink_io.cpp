// kameo - Bink's background reader without spinning while its reading is suspended.
//
// The Bink library linked into the game (BINK* sections) streams a video through a BINKIO whose Idle
// function (sub_826D9280) reads the next block of the file into a 1.5 MB buffer. A RAD timer thread at
// 80 Hz (sub_826D3818, created by sub_826D38C0) calls it through a client whose priority function
// (sub_826CFDA0) asks for service again and again while the buffer is less than half full
// (CurBufUsed * 100 / CurBufSize < 50), and once per tick otherwise; the timer's loop (sub_826D3690)
// locks the client's mutex, calls Idle, unlocks and asks again until the priority is 0.
//
// During the game's videos (the "PRESS START" background, the opening of the first level) the reader is
// suspended (BINKIO.Suspended, set through BGControl) nearly all the time, and the video data is read
// in 1 MB bursts while it is not. Idle returns at once without reading while Suspended or ReadError is
// set, but the priority function looks only at the buffer, so whenever the buffer drops below half the
// timer thread spins on mutex wait / Idle / release (host kernel calls) until the next burst fills it:
// up to a whole core in bursts, which shows as a quickly changing CPU temperature.
//
// The hook makes the priority function treat a suspended (or failed) reader as at least half full:
// it is then serviced once per tick, and Idle runs as before on the next tick after the game resumes it.

#include <rex/cvar.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include <cstring>

REXCVAR_DEFINE_BOOL(kameo_bink_io_fix, true, "Kameo",
                    "Videos: Bink's background reader asks for service once per tick while its reading "
                    "is suspended, instead of spinning (false = the original code)");

namespace {

uint32_t Load32(const uint8_t* base, uint32_t address) {
  uint32_t value;
  std::memcpy(&value, base + address, 4);
  return __builtin_bswap32(value);
}

const uint8_t* GuestBase() {
  auto* kernel = rex::system::kernel_state();
  return kernel && kernel->memory() ? kernel->memory()->virtual_membase() : nullptr;
}

}  // namespace

// [[midasm_hook]] 0x826CFDB8 (sub_826CFDA0, the Bink IO client's priority): before "cmplwi cr6,r11,50",
// r11 = the buffer use in percent and r3 = the client (BINK + 500), so the BINKIO at BINK + 240 is at
// r3 - 260: ReadError at r3 - 228, Suspended at r3 - 180.
void KameoBinkIoPriorityHook(PPCRegister& r3, PPCRegister& r11) {
  if (r11.u32 >= 50 || !REXCVAR_GET(kameo_bink_io_fix)) {
    return;
  }
  const uint8_t* base = GuestBase();
  if (!base) {
    return;
  }
  if (Load32(base, r3.u32 - 180) != 0 || Load32(base, r3.u32 - 228) != 0) {
    r11.u32 = 50;
  }
}
