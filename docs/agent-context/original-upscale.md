# Original-based local upscale production

`tools/textures/texture-upscale-originals.py` reads original pixels through
`texture-variants.original`, verifies catalog payload SHA-256 and the pinned XBE
format dispatch, and builds a resumable 2x candidate pack under ignored
`authored/`. It does not generate redesigned artwork or change saved settings.
An explicit local Real-ESRGAN ncnn executable/model folder supplies 4x color
inference, reduced to 2x. Known color categories use the model; unknown, UI,
reflection/effect and A8/L8/A8L8 sources use conventional original resampling.
Alpha comes independently from original samples. Channel data, tiny UI and
thin patterns preserve original RGBA texels with NEAREST resampling and RGBA8;
other color textures use bilinear alpha and lossless RGBA8 when alpha is present.
Fully opaque DXT1 color sources may use DXT1. All outputs have complete mips.
`texture-originals.json` retains support for excluded keys and now explicitly
approves all 164 reviewed 64x64 color icons for neural 2x upscaling. Approval
cannot overlap retained keys, name unknown keys or target channel/data formats.

Bounded directory batches amortize model loading. Oversized originals use
256-pixel tiles with 24-pixel overlap and central-region assembly in RAM.
Scratch PNGs remain under the shared 256 MiB temporary budget; candidate bytes
plus metadata stay under 6 GiB. Catalog/model receipts and per-key hashes support
restart, while mismatches/incomplete pairs require inspection. No automatic
activation or publication occurs. Compiled assets require explicit user approval
for publication; original archives and private provenance remain local.
Offline audits and selected comparisons do not establish full scene fidelity.

Synthetic checks in `tests/textures/test_texture_upscale_originals.py` cover
source hash/path rejection, alpha preservation after DDS encoding, mip/hash
validation, no-overwrite and budget refusal, specialist/data routing, scratch
ownership and overlap tile assembly. Shared native CI runs them without models,
game assets or network. Windows GPU production and Mac execution are separate
validation scopes.

`tools/textures/texture-activate-originals.py` explicitly validates complete
candidate scope against its matching catalog, then archives the prior active
pack and activates the original-based candidate. An optional `--installation`
creates same-volume hard links in the launcher Textures folder and changes only
`launcher.enhanced_textures` in its controls document, retaining a prior copy.
It refuses running target games, incomplete candidates, existing staging or
recovery paths and concurrent settings edits. Source/pack moves roll back on
failure. Activation tests cover recovery, hard links, preference preservation,
partial-scope rejection and a failed move. A separate game may keep running.

Inference uses bounded OpenMP workers (4, passive wait) and ncnn 1:2:2 queues;
model processes have a 90-second timeout. An exclusive production.lock prevents
concurrent candidate writers. A stale lock after a crash requires owner
inspection before removal; normal exit/errors release it.

The independent first-pass audit identified two black neural outputs and two
corrupted thin strips; format validity alone did not catch these. Neural
encoding now rejects visibly blank results or original-scale mean RGB error
above 40. Small/thin color inputs and pixel-sensitive data avoid the neural
model. `--review-overrides` accepts documented per-key conservative fallbacks
only; it cannot introduce extra AI categories. Changed-route/alpha-policy
candidates require explicit retirement before resuming, preserving rejected
results and the first audit as evidence. Original-source and per-file DDS
hashes permit reuse of unchanged alpha proofs after a correction batch.

After activation the generation tool refuses the same production directory,
preventing accidental duplicate packs. Start a new candidate task explicitly
for another production pass. Activation preserves the prior authored pack;
installed hard links and unchanged settings fields are verified independently.

A private selective Hotel experiment retains 4x model output for sixteen
environment keys, with unchanged entries hard-linked to the 2x pack. The shared
production CLI remains 2x; this experiment does not change its scale contract or
activate a new pack. Offline checks and Windows runtime dimension receipts pass;
the 1080p visual gain is subtle. See STATUS for scope and comparison limits.

An explicit user-authorized private icon preview covers all 164 character keys:
64x64 originals become 128x128 via the same model/alpha policy, with native DDS
validation and decoded before/after galleries. It is a separate private helper,
followed by approved additive activation. Active/installed packs now include
these 164 icons (6,812 total pairs), while Classic still uses original archives.
The production planner honors explicit icon approval from policy; generic UI
remains conservative. Regression coverage verifies approvals, exclusions and
invalid/overlapping/data-key refusal. Historical 6,648-key activation receipts
are immutable; private `active-pack.json` points to the current complete plan
and additive icon receipt. Earlier image-generation examples are illustrative
and are not these Real-ESRGAN results. See STATUS for evidence and review limits.

The user approved this complete pack as the replacement optional Enhanced
release. Exported keyed DDS/JSON use Git LFS and sanitized metadata. The current
large export uses temporary same-volume hard links and Git's persistent LFS
filter, then restores verified pointer-only working files. Every index pointer
must match the source SHA-256/size and source payloads are rehashed afterward.
The release publisher preserves prior immutable versions and streams one bounded
ZIP at a time; launchers need the new download manifest merged into main.

`texture-release.find_release` falls back to authenticated paginated release
listings when a draft's tag endpoint is absent. This preserves resumable drafts
instead of attempting duplicate creation; remote verification shares the lookup.
The release regression suite covers a draft found beyond the first listing page.
Default release copy now says Enhanced textures and refers to actual dimensions
in metadata, avoiding incorrect blanket 4K/redesign claims for 2x upscale packs.
Final publication explicitly supplies the intended tag as well as `draft=false`,
preventing an untagged release when GitHub finalizes a draft without an existing
tag. The publication transaction regression checks the resulting exact tag.
