# Texture overhaul and local preview

[Back to the README](../README.md)

This branch contains **2,046 replacements**, about **1.72 GiB installed**, covering
approximately 30% of the 6,812 known supported keys. The menu progress and
remaining gaps are recorded in [Menu texture overhaul](main-menu-textures.md).
These additions enter optional texture downloads after integration and publication. Major surfaces use up to 4K; smaller
assets use appropriate sizes. Metadata records resampling. Original game files,
source artwork and private prompts are excluded. This is a partial overhaul. Character selection/profile icons retain their original artwork; character model materials remain in scope.

## Install through the launcher

The upcoming shared Windows/Mac launcher offers **Game settings → Graphics &
resolution → Check texture downloads → Download / update textures**. Normal game
packages omit the pack. Existing bundled packs remain installed and usable.
The launcher compares file hashes, skips matching parts, downloads changed parts
and merges their keys. Matching official keys are updated; unrelated custom
textures, saves and settings remain. Downloads require confirmation, including
when opted into checks on opening Settings. Cancelled updates keep completed parts
and restore the part in progress. Classic remains the default; save the Enhanced
checkbox separately to enable replacements on launch. See [launcher usage](launcher.md).

Classic defers new replacement reads and uploads. Selecting Enhanced starts
loading replacements as their textures are used, showing originals until ready.
Switching back retains the cache, so already loaded replacements switch instantly
on later toggles. Requests already submitted can finish in Classic.

## Texture build dependency

Sanitized DDS/JSON remain in `assets/textures`, with DDS stored through Git LFS.
Ordinary source builds and game packages do not hydrate or bundle them. For asset
work run `git lfs pull`; optional publication audits DDS/mips, hashes, metadata
and the 6 GiB budget. Explicit offline bundles use packager `--with-textures` or
`--texture-pack PATH`; official publishing enforces texture-free game downloads.
The nested source ZIP also omits compiled textures. Source launches can continue
to use `assets/textures` when present. Building never enables Enhanced or changes
personal settings. Players still supply their original game data.

## Developer upload timing

For a bounded developer investigation, set `TSFP_TEXTURE_UPLOAD_TRACE` to an
isolated CSV path before launching. Common RHI and legacy GL record each eligible
upload call's key, texture mode, outcome and monotonic nanosecond boundaries.
`hash_end-hash_begin` measures source-key calculation, including calls whose
outcome is `cached`. Those calls retain their existing request/cache state and
do not reach file or graphics-resource stages. `deferred` records a new Classic
key without requesting a replacement. Late Enhanced requests have zero hash
and source-byte fields because the original upload already computed the key.
`open_end-read_begin` measures opening the file;
`read_end-open_end` includes seeking, allocation, transfer and close;
`validated-read_end` covers parsing and acceptance. Image and view begin/end
fields measure CPU calls into the graphics backend, not GPU completion. GL has
one texture-upload span and no separate view. Zero fields mean a stage was not
reached. Release/dump/cleanup can contribute
to total elapsed time outside the named stages.

The trace is off by default, stops at 8,192 rows/below 4 MiB and does not change
Classic/Enhanced behavior. CSV writes occur after each measured attempt and
flush every 64 records, so they can perturb later frames. Retain source/build
identity and separate these diagnostics from clean performance runs. Use a
fresh process to restart a completed or failed trace; do not use personal save
or settings paths for automated tests. Check that the row limit was not reached
before interpreting a selected time window; cached calls now count toward it.
Older traces from `2230901` omit hashing and cached calls and lack hash columns.

## Publishing optional pack updates

1. Audit and hydrate the committed `assets/textures` pack. Keep private source art
   out of publication. Choose a new texture version; published versions are immutable.
2. Run `python tools/texture-release.py publish --version X.Y.Z`. This streams at
   most one bounded part at a time, verifies uploaded hashes and writes the public
   schema-3 file index to `config/texture-download.json`. It does not replace the
   separate repository catalog `config/texture-pack.json`.
3. Run `python tools/texture-release.py verify --remote`, review and commit the
   generated download manifest, then merge it into main. The launcher reads that
   manifest. Publish the updated Windows and Mac launchers for the new controls.
4. Subsequent updates repeat with a new texture version. File hashes let older
   bundled packs and partial downloads be reused without a historical receipt.

The checked-in `published: false` placeholder is intentional until the first
optional release is published. No pack or launcher is published just by changing
this source. Stable key-prefix buckets reduce part churn as new textures are added.
Each archive is capped at 64 MiB compressed and generated from at most 48 MiB raw;
installer validation bounds expanded content too. Optional updates do not prune
unknown keys. Game packages retain the earlier exact-hash retirement policy for
original character icons; custom-modified icon pairs are preserved.

## Preview an installed local pack

From a Windows source checkout with a built runtime and an existing pack,
double-click **Play-Textures.cmd**. It loads replacements from
`private/texture-remaster/pack`; it does not download or generate a pack.
In the game's **Settings** (or paused **Options**), open **Video Options**
and change **Textures** between **Classic** and **Enhanced** with the native
choice arrows, then select **Apply**. The texture choice lasts for this game session and remains in sync
with Windows **F10** and **PC Settings > 4K Textures**.
This is separate from render resolution. Restart after changing pack files. Ordinary **Play.cmd** starts
in Classic mode with the local pack available for switching.

For an installed Windows launcher, the pack belongs in a normal **Textures**
folder beside **TimeSplitters Launcher.exe**, with the keyed `.dds` files directly
inside it. The install action creates this folder automatically. A missing
folder or missing replacement falls back to Classic even when Enhanced is selected.
Directory junctions and symbolic links are rejected by installation validation.
Windows Cloud Files folders such as OneDrive are supported by the launcher's
cloud-tag-aware path validation; this does not permit redirected installation paths.
On one NTFS volume, developers can install file hard links inside the normal folder
to share the existing pack's disk space. This is a snapshot of installed keys:
new or replaced source files require an explicit refresh, followed by a game restart.
The game log records successful replacements as `[TEXTURE-PACK] loaded ...`.

For a native Mac launch:

```sh
./Play-Native-Mac.command --texture-pack private/texture-remaster/pack
```

Native Mac builds expose the same in-game Display setting; actual Mac interaction
and scene validation are still pending. Launch defaults remain unchanged.

The common renderer's loader accepts full-mip DXT1/DXT3/DXT5 and RGBA DDS
replacements with compatible alpha. The legacy Mac GL adapter accepts compressed
replacements; RGBA replacements fall back to originals there.
Missing, invalid or over-budget replacements fall back to original textures.
The active pack has a **6 GiB disk budget**, including metadata, and the loader
limits resident replacement textures to **512 MiB**. Source artwork and temporary
review files are separate from those limits.

## Progress and quality

Coverage depends on the locally installed pack and its matching source revision.
Local development batches are not a promise that a GitHub release contains them.
The original game remains the fallback wherever a replacement is unfinished.

Run this from the repository root to audit the local DDS files and refresh the
coverage and size plan:

```powershell
.\.venv\Scripts\python.exe tools/texture-worklist.py
```

Keep installed replacements, unfinished artwork and unapproved drafts distinct.
Projected pack sizes describe a plan, not completed coverage. Prompts and actual
source dimensions belong beside the artwork under
`private/texture-remaster/authored`; a DDS resampled to 4K is not native 4K artwork.

Offline checks cover hashes, dimensions, mip chains and storage. They do not
prove UV alignment, tiling, lighting or full-level consistency. Native Mac
replacement support has synthetic GPU coverage; visual validation of the real
pack remains pending. See [measured progress](../STATUS.md) for recorded checks.

Retain one active pack and one game extraction. Keep temporary texture outputs
below 256 MiB, and preserve selected source art, review evidence, saves and
settings. See [project storage rules](../AGENTS.md#project-storage-and-texture-work).

The worklist reports temporary logical and allocated bytes by directory. It
counts comparison reports and any new output subdirectory conservatively;
only `authored/`, the active `pack/`, and root-level catalog/provenance files
are excluded. At or above 256 MiB it writes its diagnostic report and exits
with status 2. Restore headroom before further production. A cleanup must
preserve selected visual evidence and verify any compressed or reproducible
replacement before recycling the redundant files.

## Hybrid artwork production

Issue #74 uses individual redesigns for prominent assets and faithful enhancement
for simpler materials. Text, UI, effects, reflections and character identity need
specialist review. This changes production priorities, not the audited scope.

Refresh the local audit and production queue from the repository root:

```powershell
.\.venv\Scripts\python.exe tools/texture-worklist.py
.\.venv\Scripts\python.exe tools/texture-hybrid-plan.py --batch-size 8
```

The planner writes ignored `private/texture-remaster/hybrid-plan.json`. It groups
visually classified environmental candidates by material and occurrence count,
with separate queues for individual work, specialist review, held corrections and
unclassified originals. Occurrence count is a prioritization hint, not proof of
visual prominence. The planner does not generate, approve or install artwork.

Inspect original pixels and alpha before authoring a ready batch. Use built-in
image generation with one request per asset and shared family constraints. Keep
secondary materials modest in size; reserve up to 4K for prominent surfaces.
Compile accepted candidates, review actual DDS contact sheets and repetition,
and run the pack audit. Defer failed drafts to focused correction batches instead
of repeatedly blocking the production queue. Retain exact prompts, source sizes,
resampling and review decisions locally.

The compiler requires an exact source/output aspect ratio by default. For a
reviewed opaque color source whose generated dimensions differ by at most one
pixel along the long axis, explicitly pass `--allow-one-pixel-aspect-rounding`.
Both source axes must be at least 512 pixels. The option records the source
rounding and LANCZOS resampling in metadata; it rejects larger differences,
non-opaque images, channel conversions and original-alpha overrides. It does
not crop or pad the image, and does not repair displaced UV details. Review
the actual encoded result before installation.

Keep one active pack under 6 GiB, including metadata, and temporary review/export
outputs under 256 MiB. Consolidated playtesting remains deferred until at least
90% of audited in-scope textures are installed and all categories are reviewed.
Neither batch routing nor successful DDS encoding proves visual completion.

## Preserve original alpha while reworking surface color

Some textures store cutouts or material data in alpha. When that channel should
remain unchanged, the compiler can take it from a verified original PNG:

```powershell
$alpha = 'private/texture-remaster/originals/TEXTURE_KEY.png'
$hash = (Get-FileHash -LiteralPath $alpha -Algorithm SHA256).Hash.ToLowerInvariant()
.\.venv\Scripts\python.exe tools/texture-pack.py private/texture-remaster/authored/ART.png --key TEXTURE_KEY --width 1024 --height 512 --format DXT5 --alpha-source $alpha --alpha-source-sha256 $hash
```

Use the actual catalog key, matching aspect ratio and reviewed output dimensions.
Verify the original export against its catalog payload before using it; the PNG
hash detects later changes but does not prove its game identity. This option
replaces authored alpha during format conversion, independently of color
resampling. It uses bilinear alpha resampling, records the original PNG hash,
dimensions and range, and requires color channels with DXT5 or RGBA8. Defaults
are unchanged. Do not apply it to an intentional new cutout shape or alpha-only
artwork. Review both channels and their alignment after compilation.

For a color-only cutout rework, an opaque color plate can avoid revealing bad
RGB hidden beneath generated transparency. Author natural material colors just
beyond the original cutout edges, keeping visible features registered, and use
the verified original mask for the final texture. This padding is source artwork;
the compiled texture still has the original transparency. Retain the source,
prompt and mask provenance. Do not fill missing color automatically or use the
mask to conceal a changed silhouette, atlas layout or gameplay marking.

Inspect the compiled result over a checkerboard and compare the full-size edges,
not only the transparent source preview. Very low-opacity source pixels can have
strong red/yellow RGB that becomes visible under a different mask. Batch 122
found that opacity-aware resizing alone did not remove this defect. A reviewed
opaque color plate resolved one wood cutout; this does not establish suitability
for other materials or approve their UV alignment. See the batch 122 entry in
`STATUS.md` for measured alpha error and the pending scene checks.

DXT5 alpha is lossy. Compare decoded alpha with the resampled original; use
RGBA8 at a suitable size when compression damages an important mask. Preserving
original alpha does not prove new color details align with it or establish how
the channel is used in a scene. Runtime validation remains part of the final gate.

## Publish pack updates

Keep private source artwork and production metadata under ignored
`private/texture-remaster`. Export only reviewed compiled replacements:

```sh
python tools/texture-repository.py --source private/texture-remaster/pack
git add assets/textures config/texture-pack.json
git commit -m "Update compiled texture pack"
git push
```

The exporter audits the pack, strips private paths/prompts and creates Git LFS
objects. Git push uploads them with the source revision. On the art workstation,
`--pointers-only` avoids a second working copy; run `git lfs pull` before
publishing optional assets, or package with an explicit artist-pack override.
LFS retains an object cache as well as hydrated files. Avoid hydrating unrelated
worktrees and never prune unpushed objects.

The **Repository texture pack** workflow downloads LFS content and checks every
DDS, metadata file and declared size/count. Unrelated CI jobs keep pointer-only
checkouts. Merge approved assets into main, then follow Publishing optional pack
updates above to make them available in the launcher. Ordinary game releases do
not carry the pack. Issue #74 stays open until the overhaul is complete.
