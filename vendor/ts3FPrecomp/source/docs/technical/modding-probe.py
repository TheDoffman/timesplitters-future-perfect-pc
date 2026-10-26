#!/usr/bin/env python3
"""THROWAWAY PROBE, not part of the toolkit.

Written to settle the measurements in docs/technical/modding-models-textures.md and
kept only so they can be repeated. Nothing imports it, no test covers it, and it may be
deleted the moment the numbers in that document stop being interesting.

It answers two questions by brute-force byte search:

  1. Do the textures the D3D8 replacement binds exist verbatim inside the .pak files on
     the disc, and can each one be named?
  2. Do the vertex and index buffers it draws from, and are the vertices stable between
     frames?

Usage, from the repo root, with the discs in place:

    py -3 docs/technical/modding-probe.py textures <capture.d3dcap>
    py -3 docs/technical/modding-probe.py meshes   <capture.d3dcap> [<second.d3dcap>]

Captures come from RECOMP_D3D8_CAPTURE or the F11 key; see docs/technical/shadow-mode.md.
"""
import collections
import glob
import hashlib
import os
import struct
import sys

PAK_GLOB = "games/Time Splitters 2/data/**/*.pak"


# ----------------------------------------------------------------- pak reading

def read_pak(path):
    """(whole file, [(name, position, length), ...]) for a P8CK or P4CK archive."""
    data = open(path, "rb").read()
    magic = data[:4]
    entries = []
    if magic == b"P8CK":
        _, dir_off, count, names_pos, names_len = struct.unpack_from("<4s4I", data, 0)
        names = data[names_pos:names_pos + names_len]
        for i in range(count):
            name_off, length, pos = struct.unpack_from("<3I", data, dir_off + i * 12)
            end = names.find(b"\0", name_off)
            entries.append((names[name_off:end].decode("latin1"), pos, length))
    elif magic == b"P4CK":
        _, dir_off, dir_len, _reserved = struct.unpack_from("<4s3I", data, 0)
        for i in range(dir_len // 56):
            rec = data[dir_off + i * 56:dir_off + i * 56 + 56]
            name = rec[:48].split(b"\0")[0].decode("latin1")
            pos, length = struct.unpack_from("<2I", rec, 48)
            entries.append((name, pos, length))
    return data, entries


def load_paks():
    paks = sorted(glob.glob(PAK_GLOB, recursive=True))
    if not paks:
        sys.exit("no .pak files under %r -- run from the repo root with the disc in place"
                 % PAK_GLOB)
    return {p: read_pak(p) for p in paks}


def name_bytes(paks, blob):
    """Where `blob` sits verbatim on the disc, or None.

    Returns (pak basename, file name or None, offset within that file, whole archive,
    absolute offset in it).
    """
    if len(blob) < 64:
        return None
    for path, (data, entries) in paks.items():
        at = data.find(blob)
        if at < 0:
            continue
        for name, pos, length in entries:
            if pos <= at < pos + length:
                return (os.path.basename(path), name, at - pos, data, at)
        return (os.path.basename(path), None, at, data, at)
    return None


# ------------------------------------------------------------- capture reading

def read_capture(path):
    """({texture id: texture}, [draw, ...]) from a version-5 .d3dcap.

    Only the chunks these questions need; see src/hle/d3d8_capture.h for the rest.
    """
    data = open(path, "rb").read()
    if data[:8] != b"XBRD3D8\0":
        sys.exit("%s is not a capture" % path)
    version, header_bytes = struct.unpack_from("<2I", data, 8)
    if version != 5:
        sys.exit("capture version %d, this probe reads 5" % version)
    off = header_bytes
    textures, draws = {}, []
    shader, stage = 0, {0: 0, 1: 0, 2: 0, 3: 0}
    while off + 8 <= len(data):
        kind, size = struct.unpack_from("<2I", data, off)
        off += 8
        if off + size > len(data):
            break
        payload = data[off:off + size]
        if kind == 11:                                     # TEXTURE
            tid, fmt, w, h, levels, usage = struct.unpack_from("<6I", payload, 0)
            at, level_bytes = 24, []
            for _ in range(levels):
                _pitch, _rows, nbytes = struct.unpack_from("<3I", payload, at)
                at += 12
                level_bytes.append(nbytes)
            body = payload[at:]
            textures[tid] = dict(fmt=fmt, w=w, h=h, levels=levels, usage=usage,
                                 level0=body[:level_bytes[0]] if level_bytes else b"",
                                 chain=body)
        elif kind == 7:                                    # SET_TEXTURE
            st, tid = struct.unpack_from("<2I", payload, 0)
            stage[st] = tid
        elif kind == 8:                                    # SET_VERTEX_SHADER
            shader = struct.unpack_from("<I", payload, 0)[0]
        elif kind == 10:                                   # DRAW_INDEXED_UP
            (_prim, _min_index, nverts, prim_count, _index_format,
             index_bytes, stride, vertex_bytes) = struct.unpack_from("<8I", payload, 0)
            draws.append(dict(vs=shader, stride=stride, nverts=nverts,
                              prims=prim_count, tex0=stage[0],
                              indices=payload[32:32 + index_bytes],
                              vertices=payload[32 + index_bytes:
                                               32 + index_bytes + vertex_bytes]))
        off += size
        off = (off + 3) & ~3
    return textures, draws


# ------------------------------------------------------------------- questions

def textures_report(capture):
    paks = load_paks()
    textures, _ = read_capture(capture)
    real = {t: v for t, v in textures.items() if len(v["level0"]) >= 64}
    named = missing = 0
    offsets, sources, rows = collections.Counter(), collections.Counter(), []
    for tid, tex in sorted(real.items()):
        hit = name_bytes(paks, tex["level0"])
        rows.append((tid, tex, hit))
        if hit is None:
            missing += 1
            continue
        sources[hit[0]] += 1
        if hit[1]:
            named += 1
            offsets[hit[2]] += 1

    print("capture %s: %d textures, %d with contents"
          % (os.path.basename(capture), len(textures), len(real)))
    print("  matched a NAMED file on the disc : %d" % named)
    print("  not found on the disc at all     : %d" % missing)
    print("  texel data's offset inside its file:", dict(offsets))
    print("  source archives:", dict(sources))

    whole = sum(1 for _tid, tex, hit in rows
                if hit and hit[3][hit[4]:hit[4] + len(tex["chain"])] == tex["chain"])
    print("  whole mip chain matched verbatim : %d" % whole)

    digests = collections.Counter(hashlib.sha1(v["chain"]).digest() for v in real.values())
    print("  distinct content hashes          : %d of %d" % (len(digests), len(real)))

    print("\n  first 20:")
    for tid, tex, hit in rows[:20]:
        where = "%-14s %s" % (hit[0], hit[1] or "(no directory entry)") if hit \
                else "(not on disc)"
        print("    id %3d %4dx%-4d fmt 0x%02X lv%-2d -> %s"
              % (tid, tex["w"], tex["h"], tex["fmt"], tex["levels"], where))


def meshes_report(capture, second=None):
    paks = load_paks()
    _, draws = read_capture(capture)

    print("capture %s: %d indexed draws" % (os.path.basename(capture), len(draws)))
    named = collections.Counter()
    extensions = collections.Counter()
    unnamed = searchable = 0
    samples = []
    for i, draw in enumerate(draws):
        # name_bytes ignores anything under 64 bytes: too short to be a unique match.
        if len(draw["indices"]) < 64:
            continue
        searchable += 1
        hit = name_bytes(paks, draw["indices"])
        if hit and hit[1]:
            named[hit[0]] += 1
            extensions[os.path.splitext(hit[1])[1]] += 1
            if len(samples) < 12:
                samples.append((i, draw, hit))
        else:
            unnamed += 1
    print("  index blocks over 64 bytes       : %d" % searchable)
    print("  ... named from the disc          : %d" % sum(named.values()))
    print("  ... not found                    : %d" % unnamed)
    print("  host file extensions:", dict(extensions))
    for i, draw, hit in samples:
        print("    draw %3d vs 0x%X stride %2d nv %4d -> %-14s %s"
              % (i, draw["vs"], draw["stride"], draw["nverts"], hit[0], hit[1]))

    vertex_hits = sum(1 for d in draws
                      if len(d["vertices"]) >= 256 and name_bytes(paks, d["vertices"]))
    print("  vertex blocks found on the disc  : %d (of %d over 256 bytes)"
          % (vertex_hits, sum(1 for d in draws if len(d["vertices"]) >= 256)))

    if not second:
        return
    _, later = read_capture(second)
    by_indices = collections.defaultdict(list)
    for draw in later:
        if len(draw["indices"]) >= 64:
            by_indices[hashlib.sha1(draw["indices"]).digest()].append(draw)
    same = different = absent = 0
    for draw in draws:
        if len(draw["indices"]) < 64:
            continue
        key = hashlib.sha1(draw["indices"]).digest()
        if key not in by_indices:
            absent += 1
        elif any(o["vertices"] == draw["vertices"] for o in by_indices[key]):
            same += 1
        else:
            different += 1
    print("\n  against %s: vertices identical %d, different %d, mesh absent %d"
          % (os.path.basename(second), same, different, absent))


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    what = sys.argv[1]
    if what == "textures":
        textures_report(sys.argv[2])
    elif what == "meshes":
        meshes_report(sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
