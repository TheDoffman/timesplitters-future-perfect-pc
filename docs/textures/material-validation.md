# Material validation evidence

The 2026-10-24 #74 material continuation adds 215 reviewed metal, masonry,
rock, plaster, carved, granular and wood replacements, bringing the optional
pack to 2,261 entries. Sanitized DDS/JSON and config/textures/texture-pack.json change
together; source artwork, prompts and held drafts remain private. Square sources
are 1254 pixels, downsampled to 1024, 512 or 256; rectangular sources are
1774x887 or 887x1774, usually downsampled to 512/1024 on the long edge.
One1774x887 wood source is resampled to2048x1024 for a1024x512 original; this
is not native4K artwork. RGBA8 preserves
fine detail when DXT1 loses it. Narrow wood/metal sources are 2169/2170x725 or
2172x724 padded atlases. The private compiler extracts explicitly reviewed
regions with at most 1% aspect rounding, then emits exact original 4:1 or 8:1
outputs, up to 2048 pixels long. Full source dimensions, extraction coordinates
and resampling remain in private provenance; no native 4K claim.

The loader, key mapping, optional-delivery contract, host adapters and164original
character-icon exclusions remain unchanged. Asset batches use native DDS,
mip/aspect/alpha/hash, full-pack and decoded2x2 repeat checks. Original-scale
feature profiles supplement manual review; extrema can select different bevel
edges, so preserve initial measurements and follow-up evidence. These checks
cannot establish complete contour equivalence, scene adjacency or final lighting.
