# The frame brightness investigation, and the tools it left behind

Parked 20 September 2026, on branch `feat/upstream-lifter-fixes`.

This is a handover. The brightness bug is **not fixed**; it is isolated,
measurable, and switchable, which it was not this morning. Everything below was
measured in the running title unless it says otherwise, and the places where
that distinction bit are called out, because it cost most of the day.

---

## What is on the branch

| commit | |
| --- | --- |
| `beee5f0` | **fix(lifter)**: `movsx` from `bp`/`sp` dropped the sign |
| `9a6d496` | **fix(input)**: `XInputGetCapabilities` reported every control absent |
| `770c5b9` | diag: say which shader a draw binds; dump more than two |
| `71daad6` | docs: brightness reframed as a motion blur — **superseded** |
| `f209277` | diag: read back what the screen copy landed |
| `e0ed13a` | docs: retract the black-source diagnosis |
| `b83ce2a` | diag: measure the frame before and after the full-screen passes |
| `c30a90b` | diag: a switch to drop those passes, and the numbers |

Two of those are real fixes. The rest is instrumentation and the record of
getting the diagnosis wrong twice.

---

## The one thing that was actually fixed

**`movsx r32, bp` lifted as a zero extension.** `_lift_movsx`'s register list
was missing `bp` and `sp`, and an unmatched register fell through to the plain
(zero-extended) read with no diagnostic. TimeSplitters 2's pad handler happened
to hold right-stick X in `bp`, so -32767 read as +32769 and the stick was
unusable; the title's own deadzone made it worse rather than absorbing it.

Confirmed working with a pad. Site counts in the titles lifted here: 18 each in
Marvel vs Capcom 2 and Outrun 2, 5 in TimeSplitters 2, 2 each in Black and
Burnout 2, 0 in Panzer Dragoon Orta. All were re-lifted; only TimeSplitters 2
changed behaviour, and Burnout 2 still renders, which was the regression check.

`movsx` now emits a marker for an unmatched source register instead of a
silently unextended read, and `tools/recomp/test_lifter_movsx_registers.py`
pins it, including that `movsx` and `movzx` must accept the same registers.

---

## The brightness bug: where it actually stands

The title draws three translucent full-screen quads over the finished scene
every frame. They sample a 640x480 copy of the screen and blend it back. They
remove a fixed share of the light in every frame, and that is the bug.

**It is constant, not motion-dependent.** Driven with the probes below, holding
still and then moving in one level:

| block | scene before | frame after | ratio | stdev |
| --- | --- | --- | --- | --- |
| still, 2200 swaps | 57.2 | 20.8 | 0.363 | 0.0016 |
| moving | 110–138 | 43–48 | 0.35–0.42 | 0.02–0.04 |

The ratio does not step. What changes is the scene feeding the passes, which is
the view changing as the player looks around.

**It is level-specific in size.** Ratios seen: ~0.91 on the automated startup
path, 0.367 on one level, with intermediate values elsewhere. Constant within a
level, different between them.

**The passes own it.** Same startup, one run each:

```
passes on : scene 74.4 -> frame 67.4, ratio 0.909
passes off: scene 74.7 -> frame 73.5, ratio 0.987
```

**What they are.** The texture coordinate offsets are exactly `1/640` and
`1/480` — a fixed one-texel step — repeated three times with descending alpha
(0x34, 0x3F, then 0x0A–0x19). That is the shape of a bloom or glow. Not motion
blur, whose offset would track the camera; not depth of field, since nothing
here samples depth and stage 1 is a 1x1 white texture.

**What is still unexplained, and is the actual bug.** The shader for these
draws (`6C0A8FCA` in TimeSplitters 2) computes

```
result.rgb = saturate(2 * v0.rgb * t0.rgb)   /* v0 = 0x7F7F7F, so ~ t0 */
result.a   = 2 * v0.a
```

which under SRCALPHA/INVSRCALPHA is `out = t0*a + dst*(1-a)`. With `t0` the
screen — and it is; the probe says so — that should be close to neutral. It
measurably is not. **That gap is the bug**, and nothing here explains it yet.

---

## Two wrong diagnoses, and why they were wrong

Both are recorded because both were *supported by measurements* that were
measuring the wrong thing, and the same trap is still there for the next person.

**1. "The quads blend black over the picture."** The evidence was a replay A/B
and a framebuffer probe, and both were artefacts:

- `src/replay` never calls `xbox_D3D8CopyBackBufferToTexture` — the copy is
  done in `src/hle`. So **every replay of these draws samples a black texture**,
  which the running title does not have. This also invalidates the 0.38 factor
  that the open-issues doc carried from the start.
- The first version of the framebuffer probe called
  `IDirect3DTexture8::LockRect`, which returns `tex->sys_mem`, the upload
  shadow. The copy writes the GPU resource through a render target view and
  never touches it, so it read zeros whether the copy worked or not. The
  surface's `LockRect` does the staging round-trip and gives the real answer.

**2. "It is motion-dependent."** Reported from a pad session and taken on
trust, and a whole section of the open-issues doc was rewritten around it.
Measured, there is no motion dependence. The reported direction also flipped
mid-investigation, from "moving is darker" to "moving is lighter", because both
readings were of where the camera happened to point.

**3. A third, smaller one:** the first `RECOMP_D3D8_PS_DUMP` reading picked the
wrong shader. The dump fires on a cache miss, so its order is the order shaders
are *compiled*, not drawn — the quads were the third built, not the second.

Lessons worth keeping: read a readback path before believing a readback; do not
measure screen-read effects in a replay; and a reported symptom is a hypothesis,
not a measurement.

---

## The tools

All are off unless switched on, and all are cheap enough to leave on.

| switch | what it does |
| --- | --- |
| `RECOMP_HLE_D3D8_FB_PROBE=<n>` | every n swaps, read back the screen copy the title samples and say whether it arrived. This is the scene *before* its full-screen passes |
| `RECOMP_HLE_D3D8_BRIGHT=<n>` | every n swaps, the mean of the back buffer at Swap. The scene *after* them |
| `RECOMP_HLE_D3D8_SKIP_FULLSCREEN=1` | drop the passes entirely. A measurement, and a workaround: correct brightness, no glow |
| `RECOMP_D3D8_PS_DUMP=<n>` | dump n generated shaders, each stamped with its combiner hash |
| — | `d3d8_combiners_apply()` prints `binding shader <hash>` when it changes; interleaved with the replay's `--list-draws`, that pairs a draw with its source |

Set `FB_PROBE` and `BRIGHT` to the same interval and the two numbers against
one swap are what the passes did. They drift apart over a run (one is
elapsed-time, the other modulo), so pair them by nearest swap, not by equality.

Both probes average all three colour channels. They did not always: the frame
buffer one sampled a single byte per pixel, which against a three-channel mean
overstated the passes' cost by the size of the blue cast. Any ratio quoted
before commit `b83ce2a` is too pessimistic.

---

## The crash

Reproducible, pre-existing, and unrelated to any of the above — identical fault
address and backtrace with and without the skip switch.

```
[CRASH] Access violation at RIP=..., fault addr=0xDFC21950 (read)
  esi=0xDFC11950   in sub_0012ABA0+0x139
```

Lands reliably around swap 3990, which suggests a timed or scripted event. It
follows a failed probe for `\Device\CdRom0\ob\chrs\chr01.xbr`.

**The lead.** That file is genuinely not on disc as a loose file, and falling
back to `data/chr.pak` is normal. What may not be normal is the status: the
whole `ob\` directory is absent, so the kernel answers `0xC000003A`
(PATH_NOT_FOUND) where a real disc carrying an `ob\chrs\` directory would answer
`0xC0000034` (NAME_NOT_FOUND). A title handling only the second would take the
first badly, and a wild pointer through a half-built object is what follows.

This is a hypothesis. It has not been measured.

---

## Where to pick it up

In the order I would take them:

1. **The crash.** Reproducible, lands at a fixed point, and a hard crash in
   gameplay outranks a picture that is too dark. Start by making the file layer
   answer `NAME_NOT_FOUND` for a missing file under a missing directory and see
   whether it survives past swap 3990.
2. **What the passes should compute.** The arithmetic says near-neutral and the
   measurement says otherwise; that contradiction is the whole bug. Worth doing
   with RenderDoc on the live process rather than a capture — it shows each
   draw's effect with the real textures bound, which is exactly what replay
   cannot do here.
3. **Try the skip switch by eye.** Nobody has looked at it yet. It was verified
   numerically on the startup path, where the passes only cost 9%; the levels
   where they cost 60% are untested.

Not worth doing for this bug: moving to Vulkan. What is wrong is what gets
drawn and with what blend weight, all of which is guest-side and API-agnostic.
One thing this investigation did turn up for that plan, though: capture/replay
is the intended A/B harness for a second backend, and it cannot reproduce the
screen-copy path — so any backend comparison involving screen-read effects
would silently compare two black textures.
