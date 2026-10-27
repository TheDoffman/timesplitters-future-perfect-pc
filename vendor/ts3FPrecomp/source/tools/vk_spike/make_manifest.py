"""Write the pipeline manifest vk_spike reads: one line per vertex/pixel pair.

    py -3 tools/vk_spike/make_manifest.py <spv dir> <spirv-cross> > pairs.txt

A line is  <vs.spv> <ps.spv> <location>:<components> ...  -- the vertex
shader's inputs, which the spike turns into R32G32B32A32-style float
attributes, one binding, tightly packed. Pairs are formed within a title the
way the renderer forms them: a vertex program or the fixed-function vertex
shader with a combiner or a fixed-function pixel shader.

Module names are <title>-<generator>-<entry>-<target>-<hash>.spv, which is
what docs/technical/vulkan-backend.md's corpus step writes.
"""
import collections, json, subprocess, sys
from pathlib import Path

spv_dir, cross = Path(sys.argv[1]), sys.argv[2]
PAIRS = {"nv2a_vsh": ("ps_combiner", "ps_ffp"), "vs_ffp": ("ps_ffp", "ps_combiner")}


def reflect(p):
    return json.loads(subprocess.run([cross, str(p), "--reflect"],
                                     capture_output=True, text=True).stdout)


def components(t):
    return {"float": 1, "vec2": 2, "vec3": 3, "vec4": 4}.get(t, 4)


by_title = collections.defaultdict(lambda: collections.defaultdict(list))
for spv in sorted(spv_dir.glob("*.spv")):
    title, gen = spv.stem.split("-")[:2]
    by_title[title][gen].append(spv)

for title, gens in sorted(by_title.items()):
    for vgen, pgens in PAIRS.items():
        for vs in gens.get(vgen, []):
            ins = sorted((v["location"], components(v["type"]))
                         for v in reflect(vs).get("inputs", []))
            attrs = " ".join(f"{l}:{c}" for l, c in ins)
            for pgen in pgens:
                for ps in gens.get(pgen, []):
                    print(f"{vs} {ps} {attrs}")
