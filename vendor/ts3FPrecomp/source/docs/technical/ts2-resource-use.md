# TimeSplitters 2: what the recompiled build costs, against xemu

19 September 2026. Both run the same game, on the same machine, in the same place
in it — the Siberia level, in the tunnel at the start.

## The machine

| | |
| --- | --- |
| OS | Windows 10 Pro 19045 |
| CPU | Intel Core i7-12700H, 14 cores / 20 logical, 2.3 GHz base |
| RAM | 32 GB |
| GPU | NVIDIA GeForce RTX 3050 Ti Laptop (4 GB) **and** Intel Iris Xe (shared) |

The two GPUs matter: **the recompiled build ran on the Intel Iris Xe and xemu
rendered on the RTX** (`xemu.log`: `GL_RENDERER: NVIDIA GeForce RTX 3050 Ti`),
presenting through the Intel. The GPU percentages below are therefore percentages
of different hardware and do not compare. Everything else does.

## Method

`scripts/measure_resources.ps1 -ProcessId <pid> -DelaySeconds N -Seconds 50`,
one sample a second over the window, mean and max of each. Counters, all
per-process:

- `\Process(<name>)\% Processor Time` — 100 = one logical core saturated. "% of
  total" below is this divided by 20, the logical core count.
- `\Process(<name>)\Working Set` and `\Working Set - Private` — resident, with
  and without shareable pages.
- `\Process(<name>)\Private Bytes` — committed and not shareable.
- `\GPU Engine(pid_<pid>_*)\Utilization Percentage` — summed over the process's
  engines, and the largest single engine separately.
- `\GPU Process Memory(pid_<pid>_*)\Local Usage` and `\Dedicated Usage`.

The first sample of each rate counter is discarded: it has no previous raw value
to difference against.

**Recompiled build.** `titles\timesplitters2\build\Release\timesplitters2_recomp.exe`,
built 19 Sep 17:20 from `perf/ts2-items-1-7` at 3e96f79 (96c1040 plus the
scissor forward; 881aef0 landed five minutes after the build and is not in it), run
with the working directory set to `build\Release` and
`RECOMP_VBLANK=1 RECOMP_AC97_READY=1 RECOMP_HLE_D3D8=shadow RECOMP_TRACE_BUDGET=0 RECOMP_FPS=5`,
once with the default adaptive frame cap and once with `RECOMP_FPS_CAP=0`.
Gameplay was reached with `RECOMP_INPUT_SEQ`, save profiles cleared first.
Window **t = 195–245 s**.

**xemu.** 0.8.136, started and played to the level by hand — not scripted.
Windows counters only; xemu was left exactly as configured. Window
**17:48:12–17:49:17** for everything but `Private Bytes`, which was sampled in a
second window, **17:51:06–17:51:50**, on the same process in the same place.

### Reaching the level without a human, and how to know you did

The twelve-press path in `docs/technical/second-title-bringup.md` reaches the
level by t = 75 s **only when the save folder holds no profile**. The game
writes one at about t = 40 s of every run, so a run started with the previous
run's profile still there takes a different path ("save exists, overwrite?")
and ends on the level-select screen, where it reports a perfectly steady 57 fps
that looks like gameplay. Clear (move aside, never delete) the 12-hex folders
under `games/Time Splitters 2/UDATA/4553000a/` before **every** run. Four
extra `a` presses make the path tolerant of that prompt, at the cost of a later
arrival:

```text
RECOMP_INPUT_SEQ=12000:start,16000:start,20000:start,24000:start,28000:a,34000:a,40000:a,46000:a,52000:a,58000:a,64000:a,70000:a,76000:a,82000:a,88000:a,94000:a
```

With those, the level loads at ~t = 115 s, the opening cutscene runs to
t = 180–205 s — it varies by run, so a window that has to be all level should
start late — and the level is drawing from then on. Confirm it from the stderr rather than
from the frame rate — `[HLE-D3D8] shadow:` draw totals differenced over swaps
give ~190 draws a frame in the front end and 370–470 in the level, which is the
check `docs/technical/ts2-performance-plan.md` prescribes. Both windows here were
verified that way and by a screenshot taken mid-window.

One more caveat on the capped frame rate: the same build, unsampled, holds
59.9–60.1 in the level (`ts2-performance-plan.md`, runs 68 and 69). Sampled once
a second with `Get-Counter` it reads 57.4, in the level and in the front end
alike, so the missing frames are the sampler's own load disturbing the adaptive
gate, not the title. The uncapped and xemu numbers are unaffected.

The save profile has to be gone before **every** run, not just the first: a run
writes one at about t = 40 s, so the second run down a script takes a different
menu path than the first.

## The numbers

Mean (max) over 50 s of gameplay for the recompiled build, 60 s for xemu.

| | recompiled, capped (default) | recompiled, `FPS_CAP=0` | xemu 0.8.136 |
| --- | --- | --- | --- |
| Frame rate | **57.4 fps** (56.4–58.3) | **184 fps** (173–191) | **30 fps**, steady |
| CPU, % of one core | 145 (176) | 251 (274) | 244 (284) |
| CPU, % of all 20 cores | 7.2 (8.8) | 12.5 (13.7) | 12.2 (14.2) |
| Working set | 241 MB (241) | 236 MB (236) | 723 MB (725) |
| Working set, private | 149 MB (150) | 145 MB (145) | 639 MB (641) |
| Private bytes | 245 MB (249) † | not measured | 2,400 MB (2,401) |
| GPU, sum over engines | 8.3 % (10.8) | 23.1 % (26.1) | 51.1 % (58.5) |
| GPU, largest single engine | 8.3 % (10.8) | 23.1 % (26.1) | 33.7 % (40.3) |
| GPU memory, local | 76 MB (81) | 65 MB (76) | 242 MB (249) |
| GPU memory, dedicated | 0 MB | 0 MB | 213 MB |
| GPU adapter | Intel Iris Xe, one 3D engine | same | RTX 3050 Ti (3D + copy) **and** Intel (3D) |
| CPU per frame, % of one core / fps | 2.5 | **1.4** | 8.1 |

Read across the memory rows and the recompiled build uses **about a third of
xemu's resident memory** — 241 MB against 723 MB — a fifth of its private
resident memory, and a tenth of its commitment: 245 MB of private bytes against
2.4 GB. Most of xemu's 2.4 GB is QEMU's reservation rather than pages in use,
which is why its working set is a quarter of it; the recompiled build's private
bytes barely exceed its working set because it commits little it does not touch,
and its guest address space is a file mapping rather than private commit.

† The private-bytes figure comes from a third run, same build, same switches,
same t = 195–245 s window — **and that run overlapped another session's run of
the same title on this machine**, so its CPU, GPU and frame-rate samples are
discarded and only the memory figure is quoted. Memory is the one thing
contention does not move: that run's working set (239 MB) and private working
set (149 MB) match the clean run in the table to within 1 %, which is what
licenses quoting its private bytes and nothing else from it. The run also
reached the level at t = 207 s, so a quarter of its window is still cutscene.

CPU looks like a tie and is not one. Let run free, the recompiled build spends
251 % of a core against xemu's 244 % — both keep about two and a half cores busy
— but it draws 184 frames a second where xemu draws 30. Per frame that is 1.4 %
of a core against 8.1 %, a factor of six. Held to the console cadence it spends
145 %, a little over half of what xemu spends, and still draws nearly twice as
many frames.

## Frame rate

The recompiled build prints its own: `RECOMP_FPS=5` gives an `[FPS]` line every
five seconds. In the window, capped: 57.6, 58.0, 57.4, 57.1, 57.0, 56.4, 58.3,
56.5, 58.0 — mean 57.4, which is the adaptive gate holding one present per
vblank on a 58.8 Hz clock. Uncapped: 189.3, 191.4, 187.0, 188.5, 190.4, 173.2,
182.7, 176.8, 183.2, 176.8 — mean 184. That is roughly double the 80–89 fps the
items 1–7 baseline measured on 19 Sep, because those items landed.

**xemu: 30 fps, steady.** Read from the Windows Game Bar performance overlay on
the xemu window while it was in the level, by the user. It is not from xemu
itself: its title bar is `xemu | v0.8.136` with no counter in it, `xemu.log`
records none, and its own statistics overlay was left off rather than change the
user's settings. The Game Bar figure is the rate at which the xemu window
presents, which is what the player sees. TimeSplitters 2 targets 60 Hz on
console, so 30 fps is xemu holding half the console's cadence here — whether
that is the emulator falling short or the guest dropping itself to every second
vblank is not something these counters can say.

## Caveats

- **The GPU columns are not comparable.** Different adapters, different APIs
  (D3D on the Intel iGPU against OpenGL on the RTX), and different output sizes:
  xemu was played in a window about 1210×880, the recompiled build's shadow
  renderer is about 800×600. Its 8 % of an Iris Xe and xemu's 34 % of an RTX
  3050 Ti say nothing about each other.
- **xemu was driven by hand**, so its window is 60 s of someone playing, while
  the recompiled build's is 50 s of a scripted path standing in the same tunnel.
  Player input costs more than standing still.
- **The machine is shared.** The runs reported here were made with no compiler
  running; earlier attempts that overlapped a build were discarded, and so were
  two runs that turned out to be sitting in the front end rather than the level.
  Eight idle `MSBuild.exe` node-reuse processes were resident throughout and used
  0 % CPU. The third run, the one the private-bytes figure comes from, overlapped
  another session's run of the same title; everything but its memory counters is
  dropped for that reason. Two earlier attempts that overlapped a compile, and
  two that turned out to be parked in the front end, were rerun rather than
  reported.
- `Working Set - Private` is resident private memory and `Private Bytes` is
  commitment; they are not the same number and for xemu they differ by a factor
  of nearly four.
- The recompiled build's memory model maps guest address space through a file
  mapping, which is shareable and so does not appear in the private rows.
