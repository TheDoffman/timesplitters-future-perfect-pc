# Texture overhaul and local preview

[Back to the README](../README.md)

The alpha contains **1,505 replacements**, about **1.08 GiB installed**, covering
approximately 22% of known supported keys. Major surfaces use up to 4K; smaller
assets use appropriate sizes. Metadata records resampling. Original game files,
source artwork and private prompts are excluded. This is a partial overhaul.

## Install through the launcher

Install or update the game through the launcher. New game-version packages
include the pack in `Textures/`; no separate texture release is required.
Launcher 1.2.22 or newer supports these game packages. The launcher bootstrap
stays small. In game, select **Display settings → Textures → Enhanced**.
Classic remains available. Game updates seed missing texture pairs and preserve
existing replacements, saves and settings. To replace existing custom pairs,
use **Install pack ZIP** or **Install pack folder**, with the game closed.
The old download button explains that textures now come with game updates.

## Texture build dependency

Compiled DDS files live in `assets/textures` through **Git LFS**, alongside
sanitized JSON metadata. An LFS-enabled clone supplies them. Existing checkouts:

```sh
git lfs install
git lfs pull
```

Windows and Mac builds audit repository textures, rejecting unresolved pointers,
invalid DDS/mips, mismatched hashes and packs over 6 GiB. No separate release
is downloaded. Both packagers stream textures into the game ZIP and record
hashes in `BUILD.json`, without making a staging copy. Use `--texture-pack`
for an explicit artist-pack override. Source launches select repository textures.
Mac `--texture-pack` overrides the source launch selection.

Lightweight builds accept `-SkipTextures` (Windows) or `--without-textures`
(Mac); both packagers accept `--without-textures`. Players still supply original
game data. Building does not enable Enhanced or change personal settings.

## Preview an installed local pack

From a Windows source checkout with a built runtime and an existing pack,
double-click **Play-Textures.cmd**. It loads replacements from
`private/texture-remaster/pack`; it does not download or generate a pack.
In the game's **Settings** (or paused **Options**), open **Display settings**
and change **Textures** between **Classic** and **Enhanced** with the native
choice arrows. It takes effect immediately; **Apply settings** is for
resolution/FOV. The texture choice lasts for this game session and remains in sync
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
building that checkout, or package with an explicit artist-pack override.
LFS retains an object cache as well as hydrated files. Avoid hydrating unrelated
worktrees and never prune unpushed objects.

The **Repository texture pack** workflow downloads LFS content and checks every
DDS, metadata file and declared size/count. Unrelated CI jobs keep pointer-only
checkouts. Merge approved updates into main; the next normal game-version
release packages the committed textures. Do not publish separate texture
releases. Issue #74 stays open until the overhaul is complete.
