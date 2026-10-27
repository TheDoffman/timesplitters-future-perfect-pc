# Game enhancements: toolkit or title project

Written 23 September 2026, when several titles had reached in-game and the question
became what a per-game project built on xboxrecomp should add.

A **title project** here means its own repository with xboxrecomp as a submodule,
existing to make one game better than it was on the console. This document sorts the
enhancements such a project would want into what belongs in **xboxrecomp** (one
implementation, every title gets it) and what belongs in the **title project** (facts
about one game that no toolkit can derive), and says where each stands today.

The rule used for the split is the one `widescreen-and-resolution.md` applies item by
item: *if a piece of work contains no fact about a particular title, it is toolkit.*
Most enhancements turn out to be a toolkit mechanism plus a small per-title part that
tells the mechanism something only the game knows.

## Summary

| # | Enhancement | Today | Toolkit | Title project |
|---|---|---|---|---|
| 1 | Higher frame rates | Frame cap exists (adaptive 60 / 60 / 30 / off); above 60 is unsafe; frame interpolation shows 2-4x with logic at 60 (`frame_interp`) | Pacing, cap settings, frame interpolation | Which registers hold its matrices; or making the game's logic correct at a higher rate: the bulk of the work |
| 2 | Native widescreen | Console flag and 16:9 presentation exist; Hor+ is experimental | The flag, 16:9 presentation, Hor+ mechanisms | Which register holds the projection, which draws are HUD, 4:3-authored 2D |
| 3 | Internal resolution scaling | `RECOMP_RES_SCALE=1..8` exists | All of it | Fixes for passes that break at scale (post effects, read-backs) |
| 4 | MSAA / supersampling | Supersampling exists (item 3); MSAA only when the title asks for it | Forced MSAA, resolve, SSAA via item 3 | Passes that must not be multisampled |
| 5 | Filters / shaders | Anisotropic filtering (`RECOMP_ANISO`); no post-process chain | A post-process chain on the final frame | Game-tuned presets, if any |
| 6 | Online play replacing Xbox Live | Nothing yet; XNET/XONLINE code runs lifted | System link over a virtual LAN, a Live-shaped service stub | Matchmaking and session rules per game, server if needed |
| 7 | Model / texture / audio replacement | Designed (`modding-models-textures.md`); dump exists only in the LLE executor | Overlay filesystem, hashed texture dump and replace, audio replace | Asset naming, model formats, anything that adds content |
| 8 | FPS display | Done: F9, `RECOMP_FPS_OVERLAY=1` | All of it | Nothing |
| 9 | Native rendering (Vulkan) | Planned (`vulkan-backend.md`); D3D11 today | All of it | Nothing, unless the game draws through its own push buffers |
| 10 | Skip cutscenes | Movies: `RECOMP_SKIP_VIDEO`, `RECOMP_XMV_PLAY=0`; in-engine: nothing | Skipping pre-rendered movies, a skip key | In-engine cutscenes: where they start and how to jump past safely |

Two items are pure toolkit (8, 9). One is almost entirely per-title (1). The rest split,
and in each the toolkit part is the larger, and the part to build first.

---

## 1. Higher frame rates (60-144, or uncapped)

**Today.** The flip gate in `src/kernel/xbox_memory_layout.c` paces presentation to the
vblank. Adaptive 60 is the default, F10 steps through adaptive / 60 / 30 / off, and
`RECOMP_FPS_CAP` sets it (`resolution-and-framerate.md`). TimeSplitters 2's Siberia runs
at 5 ms a frame, so the host has room for much more than 60.

**Why this is mostly per-title.** Games of this era step their simulation per frame or
per vblank, and running faster breaks them in ways that are specific to each game.
Burnout 2 unthrottled took three different faults in three runs
(`performance-60fps.md`): an error path after a null indirect call, an integer divide
by zero where a frame-time delta came out as 0, and a wild pointer. None of those is a
toolkit bug; each is the game meeting timing it was never tested against.

**Toolkit part.**
- Caps other than 60: 120 and 144 are the gate with a different release rate, if the
  vblank the game sees is decoupled from presentation.
- A presentation-only mode: keep the game's logic at its own rate and present
  interpolated frames above it. **Done (Oct 2026)** for titles drawn through the shadow
  renderer with vertex programs: `RECOMP_FRAME_INTERP` / `frame_interp`, see
  `frame-interpolation.md`. The title project's part is naming the registers that hold
  its projection and matrices (`xbox_D3D8SetInterpRegisters`).

**Title project part.** Finding every fixed-step assumption (physics steps, animation
per frame, timers counted in vblanks) and making each one time-based, or accepting logic
at 30/60 with presentation above it. This is the largest item on the list for any game
where it is attempted, and where this list is least toolkit-shaped.

## 2. Native widescreen (not stretched)

**Today.** `widescreen-and-resolution.md` is the full analysis. `RECOMP_WIDESCREEN`
sets the widescreen bit the game reads through `XC_VIDEO` and presents the frame at 16:9.
Xbox widescreen is anamorphic (the game renders a squeezed 640x480 frame for the TV to
stretch), so for a game with its own 16:9 mode that is the whole job. A game without one
still draws 4:3 and comes out stretched, which is why the switch is off by default.
`RECOMP_HOR_PLUS` widens the field of view by scaling one vertex constant register as it
is uploaded, and `RECOMP_HOR_PLUS_REG` says which register (60 for TimeSplitters 2).
That is experimental, and it scales the HUD's projection too if the HUD shares it.

**Toolkit part.** The console flag and 16:9 presentation (done); the register-scaling
mechanism (done); the same for games using the fixed-function transform, where the
projection matrix is visible at `SetTransform` and needs no per-game register; and
drawing a 4:3-only game centred with bars, as an alternative to stretching.

**Title project part.**
- *A game with its own 16:9 mode* (Burnout 2) needs nothing: the flag is the whole job.
- *A 4:3-only game with vertex programs* (TimeSplitters 2) needs its projection register
  found, and its 2D layer (HUD, menus) kept at 4:3 and centred, or re-laid out. Deciding
  which pre-transformed draws are HUD and which are full-screen (a fade and a health bar
  are the same kind of draw) is only knowable per game.

## 3. Internal resolution scaling

**Today.** Done in the toolkit: `RECOMP_RES_SCALE=<1..8>` renders the scene at a
multiple of the guest's size and resolves it onto the window (`src/d3d/d3d8_display.c`).
Every draw path ends in normalised coordinates, so the scene is re-rasterised, not
magnified.

**Title project part.** Only what breaks at scale in a particular game: an offscreen pass
sized in guest pixels, a post effect that samples by texel offsets, or a read-back of the
frame. These show up as specific visual bugs and are fixed per game, or generalised when
two games share one.

## 4. MSAA or other supersampling

**Today.** Supersampling is item 3. MSAA is created only when a title asks for it at
device creation (`d3d8_msaa_sample_count` in `src/d3d/d3d8_device.c`).

**Toolkit part.** A forced-MSAA setting (sample count on the scene target, resolved
before presentation), off by default.

**Title project part.** Games that render to textures and read them back (depth-based
effects, render-to-texture reflections, the colour grading Future Perfect does in its
swap callback) can need particular passes left single-sampled. Which passes those are is
per game.

## 5. Filters and shaders

**Today.** Forced anisotropic filtering exists (`RECOMP_ANISO`). There is no
post-processing chain.

**Toolkit part.** A post-process pass on the resolved frame, after the game and before
the overlay: sharpening, FXAA/SMAA, CRT and scanline shaders, colour correction. It sits
where the movie layer and the F9 counter already draw (`d3d8_movie.c`,
`d3d8_overlay.c`), so the pattern exists. Loading user shaders is a toolkit feature too.

**Title project part.** Presets tuned to a game's look, if wanted. Nothing required.

## 6. Online play replacing Xbox Live

**Today.** Nothing. The XNET and XONLINE sections run lifted; Future Perfect reaches its
network set-up and hits unresolved calls there.

There are two different problems:

- **System link** (LAN play) is plain networking through the Xbox's network stack. Many
  games of this era support it. Tunnelling it between players over the internet (as
  XLink Kai does for real consoles) needs no knowledge of the game: bridge the game's
  packets to a virtual LAN.
- **Xbox Live** is a service the game authenticates with and asks for matchmaking,
  friends, scores and content. The servers are gone. Replacing it means answering the
  XONLINE API with a stand-in: accounts, presence, a lobby.

**Toolkit part.** The network layer: the kernel and XNET calls a game makes, mapped to
host sockets; a system-link bridge; and a Live stand-in that answers sign-in and the
common calls generically, so a game gets through its online start-up.

**Title project part.** Anything a game uses Live for beyond sign-in: its matchmaking
rules, its session format, leaderboards, downloadable content. A server if the game
needs one. For system-link games the project part can be close to nothing.

**Order.** System link first: it is toolkit work, and it gives real multiplayer for
every game that has it.

## 7. Model, texture and audio replacement

**Today.** `modding-models-textures.md` designs this for TimeSplitters 2 and concludes
that for textures *the generalisable work is the majority*. Its first deliverable, a
content-hash texture dump and a hash-keyed replacement folder, is not built on the
shadow renderer; a dump exists only in the LLE push-buffer executor
(`src/kernel/nv2a_pb_exec.c`).

**Toolkit part.**
- An overlay filesystem, so a mod folder shadows the game's files without editing them.
- Texture dump and replace, keyed by content hash, at `host_texture()` in
  `src/hle/hle_d3d8_texture.c`. Works on any game on day one.
- Audio replacement at the same kind of boundary: DirectSound buffers and streams, and
  movie sound, keyed by hash.
- Mesh replacement for simple vertex layouts.

**Title project part.** Naming (turning hashes into names from the game's archives),
the game's own model formats, and anything that *adds* content (a new character) rather
than replacing it, which is entirely per game.

## 8. FPS display

**Done, toolkit.** F9 toggles the counter, F10 steps the cap, and `RECOMP_FPS_OVERLAY=1`
starts with it on (`src/hle/hle_d3d8.c`, `src/d3d/d3d8_overlay.c`). Nothing per-title.

## 9. Native rendering (fully Vulkan)

**Planned, toolkit.** `vulkan-backend.md`: an RHI inside `src/d3d`, the existing HLSL
generators compiled to SPIR-V with DXC, and frame capture/replay as the bring-up loop.
Nothing per-title for games whose drawing reaches the D3D8 boundary.

**The exception** is games that fill push buffers themselves: Breakdown and XGRA draw
their menus that way today, so the host sees none of it under D3D11 either. That is the
toolkit's next renderer problem (an LLE path for push-buffer draws), not a Vulkan one,
and any title project for such a game depends on it first.

## 10. Skip cutscenes

**Today.** Pre-rendered movies can be skipped wholesale: `RECOMP_SKIP_VIDEO` refuses to
open the files and `RECOMP_XMV_PLAY=0` reports each movie finished at once. There is no
skip key and nothing for in-engine cutscenes.

**Toolkit part.** A skip key for movies: the movie replacement reports end of file when
it is pressed. Works for every game that plays XMV through the replacement.

**Title project part.** In-engine cutscenes are the game's own code and data: where one
starts, whether the game has a skip already (many do, behind a button the player never
pressed), and how to jump to its end without leaving state half-set.

---

## What a title project owns

Pulling the per-title parts together, a title project is where these live:

- The game's `recomp_manual.c`, seeds and extra symbols, until they can move upstream.
- Its frame-rate work (item 1): the logic fixes that make higher rates correct.
- Its widescreen specifics (item 2): projection registers, HUD rules.
- Per-pass exceptions for scaling and MSAA (items 3, 4).
- Its online rules beyond sign-in (item 6).
- Asset naming and format tools for its mods (item 7).
- In-engine cutscene skipping (item 10).
- Its default settings: the preset a player gets for that game.

Everything else goes into xboxrecomp, so the next game starts with it. When a per-title
fix turns out to recur in a second game, it moves upstream: that is how the toolkit has
grown so far (the lahf fix, the movie replacement and the fence mirror were each found
in one game and fixed for all).
