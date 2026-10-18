# CPU usage: one core at 100 %

**Status: fixed (2026-10-18).** Two causes: the game's waits for the GPU, and the video player's background
reader.

## Symptom

Task Manager shows low overall CPU usage, but the CPU runs warmer than the game justifies (about 65 °C on an
i7-13700K at the main menu). One core stays at 100 % from the title screen on. During videos (the
"PRESS START" background, the opening of the first level) the CPU temperature also goes up and down
quickly.

## Cause 1: the waits for the GPU

The game's Direct3D library (part of the Xbox 360 SDK, compiled into the game) waits for the GPU by
spinning: it rereads a value the GPU writes back until the GPU has caught up. There are four such wait
loops (fences, ring buffer space and two more), and each one calls the hang check `sub_820D6D30` on every
iteration. On the console that costs nothing; on a PC it keeps the game's main thread at 99 % of a core
doing no useful work.

Measured from outside the process with a per-thread CPU meter and an instruction-pointer sampler: at the
main menu the whole process used 1.10 cores, the main thread alone 0.99, and 95 % of its samples were in
the fence wait (`sub_820CDD80`) and the hang check.

**Fix:** [`src/gpu_wait.cpp`](../src/gpu_wait.cpp) plus five `[[midasm_hook]]` entries in
[`kameo_functions.toml`](../kameo_functions.toml) (the start of each of the four loops, and the hang
check): the first 100 µs of each wait still spin (short waits behave as before); after that the thread
sleeps in 250 µs slices on a high-resolution Windows timer. Same approach as POTCAWERecomp's and
GRAWRecomp's `src/gpu_wait.cpp`.

## Cause 2: the video reader

The videos are Bink files (1280×720, about 1.3 MB/s). Bink's 80 Hz timer thread fills the video's 1.5 MB
read buffer: while the buffer is less than half full it calls the reader again and again, otherwise once
per tick. In this game the reader is *suspended* nearly all the time (the data arrives in 1 MB bursts while
it is not), and a suspended reader returns at once without reading. Nothing else in that loop checks for
the suspension, so every time the buffer drops below half, the timer thread spins on mutex waits and
releases until the next burst: up to a whole core, on and off, hence the quickly changing temperature.

**Fix:** [`src/bink_io.cpp`](../src/bink_io.cpp) plus one `[[midasm_hook]]` in Bink's priority function
(`sub_826CFDA0`): a suspended (or failed) reader counts as half full, so it is called once per tick, as
when the buffer is full enough. The data is read at the same rate as before, and the buffer stays above
20 %.

## Results

| 60 FPS, 1280×720 window | Before | After |
|---|---|---|
| Main menu | 1.10 cores (main thread 99 %) | 0.18 cores |
| First level (Thorn's Castle) | 1.51 cores (main thread 97 %) | 0.83 cores (main thread 31 %) |
| "PRESS START" video | 0.68 cores (video reader 43 %) | 0.29 cores |
| First level's opening video | 0.72 cores (video reader 44 %) | 0.28 cores |

The video rows compare the build with fix 1 and without / with fix 2. Frame rate unchanged (60 FPS), the
intro and the videos take as long as before. At the main menu the CPU package power reported by the
overlay went from about 63 W to 23 W.

## Options

| Option (`kameo.toml` or `--name=value`) | Default | Meaning |
|---|---|---|
| `kameo_gpu_wait_spin_us` | 100 | Busy-wait this long before sleeping |
| `kameo_gpu_wait_sleep_us` | 250 | Sleep per check after that; `0` restores the original busy wait |
| `kameo_bink_io_fix` | true | `false` restores the original video reader loop |
