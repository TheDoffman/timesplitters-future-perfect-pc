# Burnout 2 frame rate: checking "The 40 Hz Ceiling"

A four-page brief dated 18 September 2026 ("The 40 Hz Ceiling") said that the
recompiled Burnout 2 runs at about 15 fps, that host drawing on or off makes no
difference, that the kernel's vblank pump could never deliver more than 40 Hz
because it paced itself with `GetTickCount64() + 16`, and that after moving
the clock to `QueryPerformanceCounter` the rate stayed at ~14 fps -- so the
remaining cost must be on the guest side: lifted code, the kernel bridge, the
register model. It recommended profiling the guest side and then deciding
about the register-model rewrite.

None of the brief's instruments existed in this tree. This document records
what was built to check it, what was measured, and what the measurements say.
Every number here is from a Release build of Burnout 2 on one machine
(Windows 10 19045, Intel integrated GPU), run unattended by
`scripts/run_and_report.py`, and every frame rate is labelled with what the
title was showing, because the front end alone spans a factor of thirty.

## Instruments

| Switch | What it does |
| --- | --- |
| `RECOMP_FPS=<seconds>` | Counts the title's own `D3DDevice_Swap` at the HLE boundary before the replacement does anything, and vblanks delivered to the title's ISR. One `[FPS]` line per window with the run mean. `src/kernel/recomp_fps.c`. |
| `RECOMP_SAMPLE=<hz>` | A sampling profiler: every thread's instruction pointer and a few native frames, tallied by symbol and category, reported every `RECOMP_SAMPLE_REPORT` seconds and dumped to `RECOMP_SAMPLE_DUMP`. `src/kernel/recomp_sample.c`. |
| `RECOMP_VBLANK_HZ=<rate>` | The vblank clock's rate (60). |
| `RECOMP_VBLANK_CLOCK=tick` | The old `GetTickCount64() + 16` pacing, kept so both clocks can be compared in one binary. |
| `scripts/fps_report.py` | Tabulates runs (mean over the run, median and best window); `--phases` labels each window with draws per frame, files loaded and the sampler's split of the guest thread. |

The entry profiler that already existed (`RECOMP_TRACE_PROFILE`) counts calls,
not time. It cannot tell a function entered once that runs for a frame from a
leaf entered a million times, which is exactly the "one hot path or diffuse"
question, so the sampler was needed. Its limits: static functions in the
runtime resolve to the nearest public symbol unless the library was built with
`/Zi` (now done for `xbox_kernel`, `xbox_hle`, `xbox_d3d8`); inclusive counts
through system DLLs are unreliable where the unwind stops early; leaf counts
are what to read.

## The frame clock: confirmed

`kernel_vblank_tick()` in `src/kernel/kernel_bridge.c` scheduled the next
vblank as `GetTickCount64() + 16`, called from a timer thread that slept 10 ms
per loop. `GetTickCount64` advances once per scheduler tick, 15.6 ms by
default, so a 16 ms deadline is met on the second tick, sometimes the third.
Measured on the legacy pacing (`RECOMP_VBLANK_CLOCK=tick`): **39.9 Hz** over
110 s in every run, exactly the brief's figure.

The clock now runs off `QueryPerformanceCounter` at `RECOMP_VBLANK_HZ`, and
the timer thread sleeps to the deadline on a high-resolution waitable timer
(`CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`, falling back to `timeBeginPeriod(1)`).
Delivered: **60.0 Hz** (59.9-60.1 in every 10 s window), and the timer thread's
own CPU time stayed under half a second per run, so the ISR and DPC chain the
brief worried about is not a cost at 60 Hz.

Also found while checking: the timer thread was started under a plain
`if (!g_timer_started)`, and two guest threads setting their first timer
together each started one. The first run of this investigation delivered
"vblank" at 86 Hz through a 16 ms deadline for exactly that reason, and ran
the title's ISR concurrently on two threads. Fixed with an interlocked flag.

## The frame rate: the brief's premise does not hold here

The brief's frame rates are 13-25 fps in all four host-drawing configurations.
Burnout 2 in this tree does not run at that rate in any state that could be
reached, and on the screens reached it is not held to the vblank: with the
legacy clock delivering 40 Hz it swapped at 75-280 fps, and the fps never sat
on 40, 20 or 13.3. The title has a wait-for-vblank loop (`sub_000CBD90`, see
below) but it returns early while a load is in progress, which on every front
end screen reached here it is; and the XDK `Swap` waits on the GPU time fence
(`D3D_BlockOnTime`), which the runtime's ack thread completes immediately. The
60 Hz the console imposed through the flip completing at vblank is missing, so
the title runs as fast as the host lets it (see "Consequences" below).

What was capping it was not the clock. The sampling profiler, on the build as
the main checkout has it (lifted with `--trace-all-entries`), showed the guest
main thread 98.6% on-CPU with **83% of its samples in `getenv`** (28,122 of
33,996 at 1 kHz), reached from `recomp_trace_enter()` -> `dump_va_once()`,
which read `RECOMP_DUMP_VA` from the environment on every lifted function
entry. Lifted game code was 2.6% of the same thread. One line caches it
(`src/kernel/recomp_trace.c`), and the same binary otherwise went from ~200 to
~1800 fps on the same screens:

| Build | Clock | Screen | Draws/frame | fps (10 s windows) | vblank |
| --- | --- | --- | --- | --- | --- |
| trace hook with per-entry getenv | tick | attract cycle (loading car models, `DReplayNXBOX.dat`) | 5 | 196-242 | 39.9 |
| trace hook with per-entry getenv | QPC 60 | attract cycle | 5 | 167-263 | 60.0 |
| trace hook with per-entry getenv | QPC 60 | title screen polling `UDATA\41430019\` once per frame | 22 | 48-72 | 60.0 |
| trace hook with per-entry getenv | QPC 60 | 4 draws/frame, no loads (after the attract; unidentified, likely the attract movie) | 4 | 18-23 | 49-59 |
| getenv cached | QPC 60 | attract cycle | 5 | 1590-2101 | 60.0 |
| getenv cached | QPC 60 | title screen polling `UDATA` | 22 | 800-900 | 60.0 |
| getenv cached | tick | attract cycle | 5 | 1453 (title quit after 17 s, see below) | 39.9 |
| no trace hook (lifted without `--trace-all-entries`) | QPC 60 | attract cycle | 5 | 1468-2106 (run mean 1734-1757) | 60.0 |
| no trace hook | tick | attract cycle | 5 | 1525-1549 | 39.9 |
| no trace hook, host drawing off | QPC 60 | attract cycle | (none) | 3393-3513 (crashed at 52 s, see below) | 60.0 |

Host drawing on or off does change the rate once the hook is out of the way
(1750 vs 3400 fps on the same screen): the brief's "renderer is not the
bottleneck" was true only because something else was. Neither figure means
anything for gameplay, since both are a front-end screen drawing five
primitives a frame.

The 18-23 fps row is the only state seen near the brief's figures, and it was
seen once, in one run, after 70 s, with the timer thread also losing vblanks
(49-53 Hz), which says the machine was saturated rather than that the guest
was slow; with the fixed hook that state was not reached again before the
title moved on. It is not evidence about lifted code.

So the brief's central inference -- drawing on or off changes nothing, so the
cost is guest-side -- was right about the location and wrong about the kind.
The cost was on the guest thread, in the runtime's own profiling hook, in a
single function. It was not diffuse, and it was not the register model.

## Where the time goes now

Sampled at 500 Hz on the build lifted without the trace hook, attract cycle
(5 draws/frame), host drawing on (`RECOMP_HLE_D3D8=shadow`), ~1750 fps, 110 s.
Guest main thread, 97% on-CPU, by category:

| Share | Category | The symbols behind it |
| --- | --- | --- |
| 56.5% | OS | `NtGdiDdDDIPresent` 25% (the host swap chain's Present, one per title Swap); `NtWriteFile` 6% (the kernel's stderr logging); `NtUserGetAsyncKeyState` 4.4% (keyboard polling by the input HLE, per frame); `NtDeviceIoControlFile` 3.3% (XInput with no pad attached); file open/query/close on the title's per-frame directory probe |
| 26.6% | lifted game code | `sub_000CBD90` 15% -- the title's own wait-for-vblank: it spins until the word at 0x5518FC changes (written by `sub_000B86C0`, a four-instruction function of the shape of a `D3DVBLANKDATA` callback copying the vblank count) or until `sub_000CB690` says there is something else to do, which during loading it does, so the loop returns at once and the title presents again; `sub_000C20E0` 1.3%; then a long tail |
| 8.4% | host D3D11 / Intel driver | the draw and present path |
| 0.5% | runtime (dispatch, icall) | |
| 0.5% | `hle_` | the HLE boundary |
| 0.4% | `d3d8_` | the host renderer's translation layer |
| 0.2% | kernel bridge | |

The same screen with host drawing off (`RECOMP_HLE_D3D8` unset, 3400 fps):
lifted code 56% of the guest thread, OS 40%, everything else under 1% each.
The lifted share rises because Present is gone, not because lifted code got
slower: the same polling loop is the top symbol.

So lifted code is a quarter of one thread with drawing on and half with it
off, and its hottest function is a wait loop, not work. It is spread over
~140 functions; the register model is not what these numbers point at.

With the per-entry `getenv` build the same view read: guest main 98.6%
on-CPU, C runtime 83% (all `getenv`), lifted 2.6%, trace hook 0.9%,
host D3D11 1.1%.

Other threads: the NV2A acknowledgement thread (`nv2a_ack_thread`) is a
`Sleep(0)` loop and burns a full core (109 s CPU in a 110 s run). It is what
makes the process "0.94 cores busy" while the guest thread waits; on a machine
with few cores it competes with the title. The kernel timer thread, which runs
the vblank ISR and every DPC, used less than 0.5 s of CPU per 120 s run at
60 Hz.

## Consequences of running unthrottled

With the hook fixed the title runs 25-30x faster than the console in its
front end, and that exposes it to timing it was never designed for:

- one run (legacy clock, cached hook) took an error path after a null
  indirect call at call 8.5 million and quit through `HalReturnToFirmware` at
  17 s;
- the scripted-input run died at 110 s with an integer divide by zero
  (`0xC0000094`) in `sub_000D9370+0x4B5`, the shape of a frame-time delta that
  came out as zero;
- the run with host drawing off, at 3400 fps, died at 52 s with a read of
  `0x8C00002E` in `sub_00109B40+0x25D` (edx = 0x8BFFFFFE, a contiguous-window
  pointer walked off its end), during a sequential file read.

None of the three appeared in the earlier runs at 50-250 fps, and all three
are in the same title state, which points at rate rather than at any one
function.

On hardware, `Swap` returns when the flip completes at the next vblank. Here
the fence mirror completes it at once. The kernel already has the vblank clock
and the fence mirror; the missing piece is to complete the flip fence from the
vblank tick rather than from the ack thread's poll, so a title that waits on
its swap waits one vblank. That gives the console's 60 fps for free on any
title that presents faster, and it is the fix for both crashes above.

## Pacing in every run reported here

The title advances per vblank where it waits for one, so a pump faster than
60 Hz would speed the game up rather than measure anything. The pump's rate
in every run above, from the per-window `[FPS]` vblank column:

- Legacy clock (`RECOMP_VBLANK_CLOCK=tick`): 39.3-40.1 Hz in every window.
- QPC clock, default `RECOMP_VBLANK_HZ` (60): 59.9-60.1 Hz in every window,
  except the four windows of one run noted above where it dropped to 49-56 Hz
  under load. No run paced above 60.
- The very first run of this investigation (before the timer-thread fix, on
  the legacy clock) showed 86 Hz in its first window because two timer threads
  were both delivering; it produced no frame-rate figure used here.
- The "run mean ... vblank 61-67 Hz" printed in early windows of the QPC runs
  was an artifact of the reporter counting vblanks delivered during boot
  against time since the first swap; the per-window figures were 60.0 and the
  reporter now counts from its own start.

The branch ships `RECOMP_VBLANK_HZ` defaulting to 60. What runs faster than
real time is not the pump: it is the title presenting 800-3400 times a second
on screens where its wait-for-vblank loop returns early because a load is in
progress, and `Swap` itself never blocking because the fence mirror completes
every flip at once. On hardware the flip completes at the next vblank and both
of those are held to 60. That is the "complete the flip fence from the vblank
tick" item below, and until it is done a front-end fps figure here is a
ceiling measurement, not a frame rate.

## What could not be measured

- **A race.** Two scripted sequences were tried, each picked up by the input
  HLE (`[PAD] synthetic input`): `start,a,a,a,a,a,a,a,a,a` (110 s, until the
  divide-by-zero crash) and `start,start,a,a,down,a,a,a` (180 s, no crash),
  both with `RECOMP_FAKE_INPUT_MS=700`. Neither left the front end: the title
  loaded menu audio and car models and never a track. Every figure here is
  therefore a front-end figure, and none of them is the game's frame rate. The
  right button sequence has to be found in xemu, or the input HLE needs a
  scripted stick as well as buttons. The brief's runs were
  "driven by the same synthetic input" over 200-300 s and may have been in a
  race; a race frame is heavier than a menu frame by a large factor, and the
  register-model question is still open for it -- but it has to be asked with
  the trace hook fixed, since that hook alone cost more than everything else
  combined.
- **The brief's build.** Its numbers were produced elsewhere; this tree had
  neither `RECOMP_FPS` nor a 60 Hz clock. The main checkout's Burnout 2 gen is
  lifted with `--trace-all-entries`, and its runs were made with
  `RECOMP_TRACE_PROFILE` set, which is the configuration that puts a `getenv`
  in every function entry. If the brief's runs were made the same way, its
  15 fps was measuring that.
- **Run means across configurations.** The front end moves between states
  (boot, attract cycle, title screen, attract movie) on its own timeline,
  faster with a faster clock, so two 120 s runs are rarely in the same state
  at the same time. Compare per-window with `fps_report.py --phases`, not by
  run means, until a race can be scripted.

## Recommendation

1. Do not start the register-model rewrite on this evidence. The measured
   cost of lifted code is a fifth of one thread on a title running at 30x the
   console's rate. The brief's argument for the rewrite rested on a number
   produced by the profiling hook.
2. Complete the flip fence from the vblank tick (kernel, ~50 lines) so Swap
   waits a vblank. Payoff: the console's pacing, and the two timing crashes.
3. Stop the per-frame logging costs when measuring: the `[PATH]` line and the
   `NtQueryAttributesFile` probe together are a fifth of the guest thread on
   the title screen. `RECOMP_KERNEL_LOG_BUDGET` already exists; the path log
   wants the same budget.
4. Make the ack thread sleep (1 ms, or wait on a signal from the pushbuffer
   writer) instead of `Sleep(0)`: one core back.
5. Cache "no controller" in the input HLE: `XInputGetState` with no pad
   attached is a device enumeration, and it was 17-22% of the thread that
   calls it.
6. Then script a race (the fake-input path needs a sequence that gets past
   the title screen; find it in xemu) and profile that with `RECOMP_SAMPLE`.
   If lifted code is then more than half of the guest thread and spread over
   hundreds of functions, that is when the register model is the question.

## Reproducing

```bat
rem build the title (see titles/burnout2/CMakeLists.txt); then, from the repo root:
set RECOMP_VBLANK=1
set RECOMP_AC97_READY=1
set RECOMP_HLE_D3D8=shadow
set RECOMP_FPS=10
set RECOMP_SAMPLE=500
py -3 scripts/run_and_report.py titles/burnout2/build/Release/burnout2_recomp.exe --seconds 120 --out-dir runs --tag qpc_shadow
py -3 scripts/fps_report.py --phases runs/qpc_shadow.err
```

Add `RECOMP_VBLANK_CLOCK=tick` for the legacy clock, `RECOMP_VBLANK_HZ=50` for
PAL, and `RECOMP_TRACE_PROFILE=1000000000` on a `--trace-all-entries` build to
keep the hook from printing 400,000 trace lines at boot.
