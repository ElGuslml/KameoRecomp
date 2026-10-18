// kameo - the game's waits for the GPU without spinning.
//
// The game's D3D (XDK library) waits for the GPU in four loops: fences (sub_820CDD80, the one the
// main thread spends its time in at the menus), ring space for new commands (sub_820CD578), and two
// others on values the GPU writes back (sub_820CD0C8, sub_820D57F8). Each one fills a small record
// on the stack (device, current GPU position, start tick) and calls sub_820D6D30 (hang check: a GPU
// hang is declared after 2000 ticks without progress) every time its condition is still false. On the
// console that costs nothing; here the main thread spent ~95 % of its time in those loops (sampled
// from outside the process) and kept one core at 100 %, which is heat and power with no useful work.
// The hooks in kameo_functions.toml spin briefly (short waits are unchanged) and then put the thread
// to sleep in small slices. Same approach as POTCAWERecomp's and GRAWRecomp's src/gpu_wait.cpp.

#include <rex/cvar.h>

#include <immintrin.h>

#include <algorithm>
#include <chrono>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

REXCVAR_DEFINE_INT32(kameo_gpu_wait_sleep_us, 250, "Kameo",
                     "Game waiting for the GPU: sleep this many microseconds per check once the "
                     "wait is longer than kameo_gpu_wait_spin_us (0 = busy wait, as on the console)");
REXCVAR_DEFINE_INT32(kameo_gpu_wait_spin_us, 100, "Kameo",
                     "Game waiting for the GPU: busy-wait this long before sleeping");

namespace {

using Clock = std::chrono::steady_clock;

// High-resolution timer (Windows 10 1803+): sleeps for fractions of a millisecond without
// depending on the system timer resolution (timeBeginPeriod).
class WaitTimer {
 public:
  WaitTimer() {
    timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                    TIMER_ALL_ACCESS);
  }
  ~WaitTimer() {
    if (timer_) {
      CloseHandle(timer_);
    }
  }
  WaitTimer(const WaitTimer&) = delete;
  WaitTimer& operator=(const WaitTimer&) = delete;

  void Sleep(int32_t us) {
    if (timer_) {
      LARGE_INTEGER due;
      due.QuadPart = -LONGLONG(us) * 10;  // relative, in 100 ns units
      if (SetWaitableTimerEx(timer_, &due, 0, nullptr, nullptr, nullptr, 0) &&
          WaitForSingleObject(timer_, INFINITE) == WAIT_OBJECT_0) {
        return;
      }
    }
    // No high-resolution timer: at least yield the core.
    SwitchToThread();
  }

 private:
  HANDLE timer_ = nullptr;
};

thread_local Clock::time_point t_wait_start;
thread_local bool t_wait_started = false;

}  // namespace

// [[midasm_hook]] 0x820CDE10, 0x820CD5D8, 0x820CD11C, 0x820D59CC: a wait loop starts (the last
// store of its hang-check record, executed once per wait).
void KameoGpuWaitBeginHook() {
  t_wait_started = false;
}

// [[midasm_hook]] 0x820D6D30: sub_820D6D30, every iteration of a wait loop (the condition is still
// false).
void KameoGpuWaitSpinHook() {
  int32_t sleep_us = REXCVAR_GET(kameo_gpu_wait_sleep_us);
  if (sleep_us <= 0) {
    return;
  }
  Clock::time_point now = Clock::now();
  if (!t_wait_started) {
    t_wait_started = true;
    t_wait_start = now;
  }
  int32_t spin_us = std::max(0, REXCVAR_GET(kameo_gpu_wait_spin_us));
  if (now - t_wait_start < std::chrono::microseconds(spin_us)) {
    _mm_pause();
    return;
  }
  thread_local WaitTimer timer;
  timer.Sleep(sleep_us);
}
