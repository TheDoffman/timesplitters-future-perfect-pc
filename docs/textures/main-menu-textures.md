# Menu texture overhaul

Issue #74, production progress on 2026-10-22. The menu overhaul is still
in progress. The accepted compiled replacements and sanitized metadata are
included in this branch under `assets/textures`, with DDS stored through Git LFS.
Source artwork, prompts and detailed provenance remain private. This update
does not install a launcher copy or publish a game release.

## Current coverage

| Scope | Offline result | Remaining work |
| --- | --- | --- |
| Original main-menu focus | All 25 keys have replacements | Final mapped appearance and animation |
| Conservative menu archive census | 574/750 keys have replacements; 164 character icons and eight functional originals intentionally retained | 4 keys need artwork or specific review |
| Character selection/profile icons | All 164 known keys retain original artwork; 159 installed replacements removed | Excluded from future production at user request |
| Functional UI/effect additions in this continuation | 13 schematic layers + 208 UI assets + 140 utility graphics + 60 effect layers | Final menu compositing/filtering review |
| Newly detailed scene images | 63 accepted: 29 at 512 square, 34 at 1024 square | Remaining map images and held geometry corrections |
| Decorative/material images | 20 accepted with original aspect ratio | Final material/UV alignment and appearance |
| Recovered higher-resolution map cards | 26 matching native 512-square originals compiled for 128-square keys | Scene validation |
| Character-model archive | 72 existing replacements among 392 candidate keys | 320 keys lack replacements; individual portrait/model links are unproven |

The census is the union of arcade l_101/l_102/l_103, arcademem l_101m/l_102m,
and loadbar archives. It includes map previews, character icons, shared effects
and some 3D atlases. Archive membership does not prove that every key appears
in a particular menu. The 392-key character census filters all catalog payloads
for chr.pak before deduplication, preserving cross-archive source membership.
Portrait completion does not establish character-model completion.

## Treatment and fidelity

The approved hybrid approach uses built-in image generation for prominent
artwork and preserves exact functional UI geometry through source resampling.
All replacements retain their runtime keys, original aspect and full mip chains.

- **Background:** corrected blue curved-room reflection, 1024 square from a
  1254-square generated source. Mean encoded luminance 91.92 versus original
  94.26 (2.48% darker); the first draft at 107.42 remains superseded.
- **Primary logo:** generated blue-silver shading from a 1774 x 887 source,
  compiled at 2048 x 1024 using verified original alpha. The letterform
  silhouette, subtitle and spacing are preserved.
- **Fonts:** both original 256-square glyph atlases are faithfully resampled
  to 1024 square. The generated font draft failed glyph-layout review and was
  not installed. These are not newly authored high-resolution typefaces.
- **Schematics, symbols, flags, gauges, controller art and utility panels:**
  original RGB LANCZOS and original alpha BILINEAR resampling, generally 4x
  for small icons and 2x for larger graphics. Exact compiled-pixel comparisons
  preserve text, layout and distinct color/alpha semantics. This adds stored
  samples, not invented native detail. A net improvement over the runtime's
  existing reconstruction filter still needs a scene comparison.
- **Map/scene images:** small cards use 512-square output and larger loading
  images use 1024-square output, both with original alpha. Layout, subjects and landmark review precede
  acceptance; held geometry drafts remain original.
- **Recovered map cards:** similarity candidates were compared visually against
  both source images. Accepted cards use exact RGB from the matching genuine
  512-square original, with the 128-square card's alpha resampled separately.
  This restores real source detail without inventing different buildings,
  camera angles or scene geometry. Generated drafts superseded by this recovery
  remain held and are not counted as accepted generated art.
- **Character portraits:** original selection/profile icons are retained at the
  user's request. The 158 previously accepted generated portraits and one older
  icon replacement were removed on 2026-10-22. Historical source artwork and
  receipts remain private; character model materials and full-size scene art
  are separate and unchanged. `config/textures/texture-originals.json` excludes all 164
  known icon keys from the production queue.

RGBA8 avoids block-compression damage to fine UI coverage. Every accepted DDS
passes native format parsing, mip/aspect/hash checks and Python lossless pixel
comparisons. Existing pack files were protected by before/after SHA-256 checks
during the bulk UI batches. Seven uniform primitives and one hard split mask
remain original because enlargement adds no useful authored detail.

## Held work and validation limits

The original wireframe has been compiled at its native 512-square size with
pixel-exact RGBA8 encoding and full mips. No generated line geometry or new
native detail is claimed; menu-wireframe-validation.json records its checks.
The remaining 4 non-icon menu assets were rejected by image-generation safety
checks. Five additional historical rejections are now intentionally original
character icons. They have not been retried through other tools or added to the pack.
Their originals remain in use. menu-generation-blockers.json lists each key,
description, original payload hash and rejection receipt. The menu overhaul
cannot be reported complete while these replacements and scene checks remain.

73 historical menu-generation records were held in the batch receipts;
4 non-icon held keys still need replacements; the five held icon keys now
retain originals by choice.
Reasons include card-edge coverage, background color drift, extra
facial features, changed scene geometry and image-generation rejections.
Those drafts are not installed or counted as accepted. The previously held
generated arrow has a separate faithful source replacement; its generated draft
was not accepted. Remaining non-icon menu materials continue in the explicit key-level queue.

No game launch was performed. Scene identity, menu readability/animation,
material associations, GPU residency and actual Windows/Mac appearance await
the near-completion validation gate. Parser success is not proof of in-game
appearance. No runtime or host-adapter change was required; MAC-011 remains
Awaiting validation.

## Storage and reproducibility

At the 2026-10-22 menu audit, the active pack contained 2,046 entries and occupied
1,844,405,700 bytes (1.72 GiB), below 6 GiB.
The 2026-10-22 texture workspace snapshot, including retained source art, occupies
9,003,558,966 bytes. Temporary files total
266,089,008 bytes, with 2,346,448
bytes below the 256 MiB cap at this audit. These are different budgets. Git LFS additionally stores the compiled objects
for upload; the checkout retains small pointers rather than another full pack.

One rotating comparison sheet is used. Ephemeral original PNG references
created in this pass were removed only after exact pixel/hash verification
against recoverable catalog spans; generated sources and recovery receipts
remain. Previously denied cleanup targets were untouched. No game extracts,
ZIPs, staging packs or builds were duplicated. Five old repetition sheets were
also retired after byte-exact in-memory reconstruction, retaining primary
comparisons, verified inputs and menu-cache-retirement.json recovery evidence.

The removal audit verified 14,760 retained DDS/JSON files unchanged across
active copies, including all 72 installed character-model materials. The public
pack occupies 1,843,682,193 bytes with sanitized metadata. No gameplay ran.

Private evidence is under private/texture-remaster:

- character-icon-removal.json: exact retired keys, prior metadata, removal
  verification and hashes for every retained file.

- main-menu-focus.json and main-menu-validation.json: original focus and
  brightness/logo/font measurements.
- menu-complete-coverage.json and menu-character-model-coverage.json:
  complete candidate lists, original source spans and explicit gaps.
- menu-overlay-02-validation.json, menu-faithful-validation.json and
  menu-utility-validation.json and menu-effects-validation.json:
  source-faithful batch acceptance and hashes.
- menu-art-*/menu-map-* generation and validation JSON records: exact prompts,
  original references, actual generated paths, accepted assets and held reasons.
- authored/*-menu*-prompt.json: retained per-asset prompts using built-in
  imagegen; pack/*.json: actual source/output dimensions and resampling method.
- menu-reference-retirement-*.json: verified original-reference recovery.
- menu-*-reference-retirement.json and menu-cache-retirement.json: additional
  verified temporary-reference and repetition-sheet recovery receipts.
- menu-map-source-matches.json and menu-recovery-*-validation.json: reviewed
  native source matching, source hashes and exact recovered DDS acceptance.
- worklist.json, hybrid-plan.json and menu-current-summary.json: current full
  pack audit, production queue and storage snapshot.

Original game data and source artwork remain private. Normal game releases
bundle the committed pack after integration; Enhanced remains a player choice.
No separate texture release is created.

## Existing installations

A launcher update carrying the retirement migration is needed to restore icons
in existing installations. Future game packages include the retired bundled
DDS/JSON hashes. During game installation, that launcher removes matching old
pairs through its normal recovery transaction; custom replacements are preserved.
Fresh installations contain no retired icon files. Merging this source change
does not itself publish a launcher or game release.
