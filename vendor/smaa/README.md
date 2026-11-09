SMAA reference implementation by Jorge Jimenez and collaborators, from
https://github.com/iryoku/smaa at commit
`71c806a838bdd7d517df19192a20f0c61b3ca29d`.

`provenance.json` pins the unmodified shader, area/search lookup headers and
license. `tools/build/embed-smaa.py` embeds that shader into the production C
header, replacing only the two effect-style sampler declarations with explicit
RHI bindings and adding `src/graphics/smaa_wrapper.hlsl`. Run it with `--check`
to verify regeneration. The reference algorithm and lookup bytes are unchanged.

The game uses SMAA 1x High with luma detection on gamma-encoded UNORM color,
zero subsample indices and no temporal accumulation/reprojection. Edge and blend
targets are cleared every frame. The lookup textures are linear R8G8/R8 UNORM.
