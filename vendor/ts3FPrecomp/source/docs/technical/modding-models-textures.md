# Adding or editing character models and textures

What it would cost to let somebody change a character's look in a recompiled title,
costed against TimeSplitters 2. This is an estimate, not a design. Nothing in `src` was
changed to write it.

The headline is one measurement: **the texture bytes the D3D8 replacement sees are the
same bytes that sit in the `.pak` files on the disc**, byte for byte, whole mip chain
included. That collapses most of the distance between the two routes. The model bytes are
not — the title converts vertices at load — but the *index* buffers are, and they are
enough to name the model a draw came from.

---

## What was verified, and how

Everything in this section was measured in this worktree, from the discs in
`games/Time Splitters 2/` and the captures in `games/_pipeline/timesplitters2/`. Every
number below can be reproduced with `docs/technical/modding-probe.py`, which is a
throwaway probe committed only so the measurements can be repeated — nothing imports it
and no test covers it:

```
py -3 docs/technical/modding-probe.py textures games/_pipeline/timesplitters2/cap_sh/sh_06600.d3dcap
py -3 docs/technical/modding-probe.py meshes   games/_pipeline/timesplitters2/cap_sh/sh_06600.d3dcap \
                                               games/_pipeline/timesplitters2/cap_sh/sh_05200.d3dcap
```

Where a claim is inference rather than measurement, it says so.

### The disc

`games/Time Splitters 2/data/` holds 25 top-level `.pak` files plus `arcade/` and
`story/` subdirectories, 773 MB in total. Magic is `P8CK` on `chr.pak`, `gun.pak` and
`sounds.pak`, `P4CK` on the rest. The published `P8CK` layout — 20-byte header of
magic, directory offset, file count, name-table offset, name-table length, then 12-byte
entries of (name offset, length, position) — parses `chr.pak` exactly: 1295 entries,
314 `.xbr` under `ob/chrs/` and 981 `.xbt` under `textures/`. No compression, no
encryption, names in clear.

`chr.pak` is the character archive: `ob/chrs/chr01.xbr`, `chr01lod2.xbr`,
`chr06_hat.xbr`, and so on up to 314 files.

The run log `games/_pipeline/timesplitters2/runs/f11.err` shows the title opening these
by name through the path layer (`[PATH] \Device\CdRom0\data\chrinc.pak`). It opens
whole archives, not files within them, so a redirect only has to catch a handful of
opens per level.

### Disc bytes versus runtime bytes — textures

Taking each `D3D8CAP_TEXTURE` chunk out of a capture and searching for its level-0 bytes
inside every `.pak` on the disc:

| capture | textures with contents | matched a **named** file on disc | not found |
|---|---|---|---|
| `cap_sh/sh_06600.d3dcap` (in level) | 102 | **98** | 4 |
| `caps/cs_06000.d3dcap` (cutscene) | 100 | **100** | 0 |

The four misses are the 640x480 and 128x128 linear surfaces the title renders into — not
assets at all. Every genuine asset matched, and the *whole mip chain* matched, not just
level 0. The texel data begins at a fixed **+128** inside its `.xbt` file in 97 of 98
cases (the one exception, `textures/misc/explosionanim8.xbt`, matched at +1728 and is
presumably a multi-frame strip).

So a runtime texture resolves to a disc filename:

```
id  56  128x64  fmt 0x0C  ->  chr.pak   textures/0267.xbt
id  11  256x256 fmt 0x0C  ->  l_100.pak textures/misc/brickdl.xbt
id   2  16x8    fmt 0x0E  ->  l_100.pak textures/misc/radarspot.xbt
```

Formats seen at the boundary are `0x0C` (DXT1), `0x0E` (DXT3), `0x06` (swizzled
A8R8G8B8) and `0x12` (linear A8R8G8B8, render targets only). No P8 anywhere in these
frames — the palettised variant is the PS2 build's.

### Content hash as an identity key

`src/hle/hle_d3d8_texture.c` keys its cache on `(va, data, format, size)` — guest
addresses, which are not stable across runs, and are reused: the file's own comment
notes a freed container's address can be reallocated. It also computes
`level0_checksum()`, FNV-1a over level 0 **sampled to 4096 evenly spaced bytes** when
level 0 is larger than that, refreshed once per frame per bound texture. That is a
change detector, not an identity key, and it is never exposed outside the file.

Measured across four `cap_sh` captures 2100 frames apart in one run:

- 101 distinct full-chain SHA1s among the 102 textures that carry contents — one
  collision, two ids holding the same asset.
- **All 102 present in all four captures, unchanged.**
- Zero `D3D8CAP_TEXTURE_LEVEL` chunks in any of them: no bound texture's texels changed
  during a frame.

The capture's own numeric ids also happened to be stable here (102 of 103 agreed across
captures), but that is an artefact of one continuous run with no level reload; the
cutscene set disagrees on 13 of 101. The content hash is the robust key, and it is free:
hashing level 0 unsampled costs 4–32 KB once, when the texture enters the cache.

### Disc bytes versus runtime bytes — geometry

The same search, against the vertex and index blocks of `DRAW_INDEXED_UP` chunks:

- **Vertex blocks: 0 of 303 found on disc.** The title converts vertices at load.
- **Index blocks: 108 of 190 (>= 64 bytes) found verbatim**, and every one of them lands
  inside a named `.xbr`:

```
draw   9  vs 0x10004 stride 28 nv  240 -> l_35_AR.pak ob/dam/fence2.xbr
draw  11  vs 0x10008 stride 36 nv  616 -> l_13_AR.pak ob/guns/silencedpistol_cl.xbr
draw  14  vs 0x10007 stride 28 nv  170 -> l_35_AR.pak bg/level35/level35.xbr
draw 103  vs 0x10008 stride 36 nv   16 -> l_35_AR.pak ob/chrs/chr36lod2.xbr
```

Matching draws between two captures by index-block hash and comparing their vertex
bytes: **189 identical, 0 different** (`sh_05200` vs `sh_06600`), and 28 identical, 0
different across two consecutive cutscene frames. The vertex buffers are built once at
load and then only transformed by constants — there is no per-frame CPU skinning on any
draw measured.

### How a character is actually drawn

In `sh_06600`, the one `chr.pak` texture bound (`textures/0267.xbt`) is used by four
draws, two of which name `ob/chrs/chr36lod2.xbr`. They run vertex program `0x10008` at
stride 36, preceded by a constant write of **four registers at c64** — one 4x4 matrix.

Across the whole frame the largest per-draw constant upload anywhere is **eight
registers** (two matrices, on program `0x1000A`, stride 48, on eight draws of 6–14
vertices each). The common case, 310 times, is two registers at c11. There is no bone
palette anywhere.

Read together with the static vertex buffers, that says characters are **rigid segments
transformed by one matrix per draw**, with small two-matrix patches at the joints. This
is the classic segmented-character construction, and it is much friendlier to
substitution than a skinned character would be.

Two honest caveats. The character in these captures is a `lod2` — a distant, 16-vertex
LOD. **I did not capture a close-up character**, and a near one may well use a different
program with a longer constant block. And 82 of 190 index blocks did not match the disc,
so that fraction of draws cannot be named this way.

### What the community has already solved

Researched separately, links in the summary at the end of this file. In short, for the
**Xbox** build specifically:

- The `.pak` container is solved twice over, with three working extractors and one
  repacker.
- `.xbt` textures are solved: format field at 0x14, values DXT1 / DXT3 / Morton-swizzled
  DXT1 normal map / raw RGB24. A Noesis plugin reads them.
- `.xbr` models are solved **read-only and static**: the same Noesis plugin reads
  positions, UVs, per-vertex winding flags and (when the leading u32 is `0xC`) normals,
  as triangle strips. Every `.xbr` in `chr.pak` has leading u32 `0xC` — confirmed here.
- **`.xbr` has no writer, on any platform, and the reader recovers no bones, no weights
  and no skeleton.** Nobody has imported new geometry into TimeSplitters 2 anywhere.
- Character *texture* replacement is a mature scene, but only via emulator texture
  substitution (Dolphin, PCSX2) — i.e. exactly the Route B mechanism, done by someone
  else's runtime.
- Character *model* replacement exists only as swapping one shipped `.xbr` into another
  character's slot.

Two consequences worth stating plainly. First, a large part of any TS2-specific work
below is *re-deriving what is already published*, and can be cut by using the existing
tools. Second, the model-authoring problem is genuinely unsolved, by anyone, and this
project would not get it for free.

---

## Effort

Days are for someone already fluent in this codebase, including testing and the
documentation the repo expects. "Toolkit" means every title benefits; "TS2" means it is
reverse engineering or tooling that transfers to no other game.

### Textures

| # | Work item | Route | Days | Kind |
|---|---|---|---|---|
| T1 | Overlay directory in `kernel_path.c`: try `<overlay>/<rel>` before `<game_dir>/<rel>`, behind an env switch | A | 1 | Toolkit |
| T2 | `P8CK`/`P4CK` unpack + repack tool in `tools/` | A | 0.5 | TS2 (or use `tspak`/`Splitter`: 0) |
| T3 | `.xbt` ↔ DDS converter, all four format codes | A | 1–2 | TS2 (or use `fmt_xbox.py` as a spec: 1) |
| T4 | Unsampled content hash at cache-entry time, exposed; `RECOMP_TEX_DUMP` writes every bound texture as DDS named by hash | B | 1–2 | Toolkit |
| T5 | Replacement loader: index a directory of `<hash>.dds`, build the host texture from it in `host_texture()` instead of from guest memory | B | 2–3 | Toolkit |
| T6 | Offline indexer: hash every `.xbt` payload in every pak, emit `<hash> -> chr.pak:textures/0267.xbt` so dumps have human names | B | 0.5–1 | TS2 |
| T7 | Extend the mirror to cube, volume and P8 textures so they can be replaced too | B | 3–5 | Toolkit |

**Route A textures: 2.5–3.5 days** (T1–T3), and the result is a permanently modified
disc image that the title loads with no runtime involvement at all.

**Route B textures: 3.5–6 days** (T4–T6), excluding T7.

### Models

| # | Work item | Route | Days | Kind |
|---|---|---|---|---|
| M1 | Slot-swap an existing `.xbr` onto another character | A | 0 beyond T1–T2 | TS2, already documented |
| M2 | In-place vertex edit: patch positions/UVs of an existing `.xbr` without changing topology or file length | A | 3–7 | TS2 |
| M3 | Full `.xbr` writer — face tables, the three geometry sections, strip generation, materials, and whatever binds segments to the skeleton | A | 15–40 | TS2, **unsolved anywhere** |
| M4 | Per-draw mesh key from the index-block hash, and an offline index mapping it to an `.xbr` name | B | 1 | Toolkit + TS2 index |
| M5 | Geometry substitution at the draw: swap vertices and indices for a matched key, keeping the title's declaration | B | 3–5 | Toolkit |
| M6 | Author round-trip: export a matched draw's vertices to OBJ/glTF in each stride/declaration combination seen, and import back | B | 3–5, plus more per new layout | Toolkit |

**Route A models: 3–7 days** for conservative edits, **15–40 days** for arbitrary new
geometry, and that larger number is the least trustworthy figure in this document.

**Route B models: 7–11 days** (M4–M6) for segment-level replacement.

### What would make these estimates wrong

- **T5 is the one that could surprise.** Substituting a texture of different dimensions
  or format should be free — sampling is by UV and `src/d3d/d3d8_resources.c` already
  maps ~120 format enumerators — but the cache in `hle_d3d8_texture.c` has three paths
  that assume the host texture mirrors the guest one: the per-frame checksum re-upload,
  the `rendered` render-target flag, and eviction by `used_swap`. Each needs a guard. If
  the title's texture *stage* state turns out to depend on size anywhere, add 2 days.
- **M3's 15–40 is a range because nobody has done it.** The `.xbr` reader skips a face
  table, and skips the skeleton entirely. Whether segment-to-bone attachment lives in
  the `.xbr`, in `anim.pak`, or in a table in the executable is unknown to me and, as far
  as the research found, to everyone. If it lives in the XBE, this becomes a lifting
  problem rather than a file-format problem and the number goes up.
- **The close-up character is unmeasured.** If a near character turns out to use a real
  bone palette, M5 and M6 stay valid (you would substitute the bind-pose geometry) but
  the reassuring "one matrix per draw" picture stops being the whole story.
- **Push-buffer draws bypass everything in Route B.** TimeSplitters 2 does not use
  `BeginPush` in the frames measured; Burnout 2 does, at two call sites. For a title
  that does, Route B simply does not see those textures or draws. Route A is unaffected.
- **T3 and T2 shrink to near zero** if existing community tools are used instead of
  being reimplemented, at the cost of a third-party dependency and a licence question.

---

## The distinction that matters: toolkit versus TimeSplitters 2

Counting the texture items: **T1, T4, T5 and T7 are toolkit** — an overlay filesystem, a
content-addressed texture identity, a replacement path in the D3D8 mirror, and mirror
coverage for the texture kinds it currently skips. None of them contain a single fact
about TimeSplitters 2. Applied to a third title, they work on day one.

**T2, T3 and T6 are TimeSplitters 2** — but they are the *cheap* part, they are already
solved publicly, and they only produce nicer names and an offline editing path. You can
ship a working texture mod without any of them.

That is the pleasant asymmetry here, and it is worth saying explicitly: **for textures,
the generalisable work is the majority of the work, and the per-title work is optional
polish.** The `.xbt` payload is already in the Xbox's own packing, so no format
knowledge is needed at the boundary — the runtime never has to know what a `.xbt` is.

For models the asymmetry inverts. M4 and M5 are toolkit and cheap; M6 is toolkit but
grows with every vertex layout a title uses. M3 — the one item that would actually let
somebody *add* a character rather than reshape an existing one — is entirely TS2-specific,
is the largest item on the page, and buys nothing for any other title.

---

## Recommended first deliverable

**A content-hash texture dump and a hash-keyed DDS replacement folder.** Items T4, T5 and
T6: roughly **4–6 days**.

Concretely, what someone would do:

1. Run TimeSplitters 2 with `RECOMP_TEX_DUMP=<dir>`. Every texture the title binds lands
   in that directory as `<hash>.dds`, alongside a manifest naming each one from the disc
   index — `a3f19c… = chr.pak:textures/0267.xbt, 128x64 DXT1, 8 levels`.
2. Open `chr.pak_textures_0267.dds`, paint on it, save it as DXT1 with mips.
3. Drop it in `RECOMP_TEX_REPLACE=<dir>` and run again.
4. The character wears it.

It is the smallest thing that closes the loop, and every measurement above says it will
work: the identity key is proven stable, the bytes are proven to be the asset's own, and
the substitution point (`host_texture()` in `src/hle/hle_d3d8_texture.c`, between
`read_layout()` and `upload()`) is a dozen lines from where the host texture is created.

### What it would not cover

- **Only what shadow mode draws.** `RECOMP_HLE_D3D8=shadow` renders into its own window,
  titled "xboxrecomp - D3D8 replacement (shadow)", beside the title's own output. A
  replacement changes that window. Whether that is the window the user considers "the
  game" is a question about how the title is run, not about this feature.
- **Cube, volume and P8 textures**, which the mirror counts and skips today (T7).
- **Textures the title renders into** — correctly so; their content comes from the host
  renderer, not from guest memory.
- **Textures bound through a hand-filled push buffer**, in titles that do that.
- **Duplicate content.** Two texture ids in one capture shared a hash. A replacement
  keyed on content replaces every texture with that content, which is usually what
  someone wants and occasionally is not.
- **Geometry, entirely.** No model changes, no new characters, no proportion changes.
- **Persistence into the disc image.** The mod lives in a folder beside the executable;
  the disc is untouched. Route A's overlay (T1) is what turns a proven replacement into
  a distributable modified pak, and it is one extra day.

---

## Sources for the community-state claims

Format specs: `github.com/GoomiiV2/TS-ReSplit` (wiki: Pak-Formats, TS2-Model,
TS2-Texture-Formats) and `github.com/RyanJGray/OpenRadical`
(`Documentation/FileFormats/OpenTS2_FileFormats_PAK.md`). Extractors:
`github.com/OpenRadical/tspak`, `Tools/frdpak`, and `Splitter` inside TS-ReSplit. Xbox
`.xbr`/`.xbt` reader: `github.com/OpenRadical/noesis` →
`plugins/python/fmt_xbox.py`. Emulator texture packs, as evidence that TS2 textures are
identifiable at a graphics boundary: `moddb.com/mods/ts2-modders-resource-pack` and
`nexusmods.com/timesplitters2/mods/12` (Dolphin, `GTSE4F`). PC-side HD replacement of
the same Xbox asset set: `github.com/HFTSRedux/TS2Redux`.

The `P8CK` directory layout, the `.xbr` leading `0xC`, and the `.xbt` format code at
0x14 were checked against the discs here and agree. Everything else in this section is
taken on the research's word.

One note for anyone repeating that research: `tcrf.net/Notes:TimeSplitters_2` served a
page that reads as a prompt-injection attempt rather than technical notes. It yielded
nothing and was not acted on.
