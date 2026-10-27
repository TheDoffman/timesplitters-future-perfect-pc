"""Audit texture production scope without extracting another game-data copy.

Archive names are routing hints, not visual asset classifications. Unknown
formats and entries remain open work; payload sizes do not prove an encoding.
"""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

import argparse
from collections import Counter, defaultdict
import importlib.util
import hashlib
import json
import struct
from pathlib import Path


def load_tool(name):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(name + '.py'))
    tool = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(tool)
    return tool


catalog_tool = load_tool('texture-catalog')
worklist_tool = load_tool('texture-worklist')

# Allocations include metadata and are planning ceilings, not filled space.
BUDGET_MIB = {
    'level_and_shared': 3584,
    'characters': 768,
    'weapons': 512,
    'mapmaker': 384,
    'interface': 256,
    'unclassified_reserve': 640,
}


def route(archives):
    """Give shared assets one production owner while retaining every source."""
    paths = set(archives)
    if 'pak/chr.pak' in paths:
        return 'characters'
    if 'pak/gun.pak' in paths:
        return 'weapons'
    if paths & {'pak/slides.pak', 'pak/loadbar.pak', 'default.xbe'}:
        return 'interface'
    if any('/tileset/' in p for p in paths):
        return 'mapmaker'
    return 'level_and_shared'


def size_hypotheses(width, height, size):
    """Report byte-count matches only; never infer channel order or swizzling."""
    matches = []
    for bpp in (1, 2, 4):
        base = width * height * bpp
        if size == base:
            matches.append(f'{bpp * 8}-bit base level')
        total, w, h, levels = 0, width, height, 0
        while True:
            total += w * h * bpp
            levels += 1
            if levels > 1 and total == size:
                matches.append(f'{bpp * 8}-bit {levels} mip levels')
            if w == h == 1:
                break
            w, h = max(1, w // 2), max(1, h // 2)
    return matches


def build(game, root):
    catalog = json.loads((root / 'catalog.json').read_text(encoding='utf-8'))
    worklist = worklist_tool.build(root)
    known = defaultdict(set)
    embedded=defaultdict(lambda:defaultdict(list))
    xbe_sources=[]
    for texture in catalog['textures']:
        for source in texture['sources']:
            if source.get('source_kind')=='xbe_section':
                xbe_sources.append((source,texture['sha256']))
                continue
            if source.get('embedded'):
                parent=(source['crc'],source['container_offset'],source['container_length'])
                embedded[source['archive']][parent].append((source,texture['sha256']))
            else:
                known[source['archive']].add((source['crc'], source['offset'], source['length']))
    xbe_summary={}
    if xbe_sources:
        verify=load_tool('texture-xbe-sections')
        sections,proof=verify.verified_sections(game/'default.xbe')
        expected={(s['name'],s['offset'],s['length'],s['virtual_address'],s['sha256']) for s in sections}
        actual={(s['section'],s['offset'],s['length'],s['section_va'],digest) for s,digest in xbe_sources if s['archive']=='default.xbe'}
        if actual!=expected or len(xbe_sources)!=len(expected):raise ValueError('Stale or duplicate XBE texture section references')
        xbe_summary=dict(verified_occurrences=len(xbe_sources),executable_proof=proof)
    archive_rows = []
    for path in sorted((game / 'pak').rglob('*.pak')):
        relative = path.relative_to(game).as_posix()
        counts, signatures, found = Counter(), Counter(), set()
        embedded_found=set()
        for f, crc, pos, length, compressed in catalog_tool.entries(path):
            counts['entries'] += 1
            identity = (f'{crc:08x}', pos, length)
            if identity in embedded[relative]:
                if compressed:raise ValueError('Embedded container became compressed')
                embedded_found.add(identity)
                counts['entries_with_embedded_textures']+=1
                for source,digest in embedded[relative][identity]:
                    start=source['offset'];end=start+source['length']
                    if start<pos or end>pos+length or start-pos!=source['relative_offset']:
                        raise ValueError('Embedded source escapes its container')
                    f.seek(start)
                    if hashlib.sha256(f.read(source['length'])).hexdigest()!=digest:
                        raise ValueError('Embedded source payload changed')
                    reference=source['table_reference']
                    if reference<0 or reference+16>length:raise ValueError('Embedded table reference out of bounds')
                    f.seek(pos+reference);pointer,_,_,flags=struct.unpack('<4I',f.read(16))
                    if pointer!=source['relative_offset'] or not flags&0x10000000:
                        raise ValueError('Embedded table reference changed')
                    counts['cataloged_embedded_texture_occurrences']+=1
            if identity in known[relative]:
                if compressed:
                    raise ValueError(f'Catalog references compressed entry: {relative} {crc:x}')
                found.add(identity)
                counts['cataloged_texture_entries'] += 1
            else:
                counts['unclassified_entries'] += 1
                counts['unclassified_payload_bytes'] += compressed or length
                if compressed:
                    counts['compressed_unclassified_entries'] += 1
                f.seek(pos)
                # Small signatures help prioritize parsers; no asset extraction.
                signatures[f.read(min(length, 4)).hex()] += 1
        if found != known[relative]:
            raise ValueError(f'Stale catalog references in {relative}')
        if embedded_found!=set(embedded[relative]):
            raise ValueError(f'Stale embedded container references in {relative}')
        archive_rows.append(dict(archive=relative, **counts,
                                 common_unclassified_prefixes=dict(signatures.most_common(8))))
    missing_archives = (set(known)|set(embedded)) - {a['archive'] for a in archive_rows}
    if missing_archives:
        raise ValueError(f'Missing catalog archives: {sorted(missing_archives)}')

    groups = {name: dict(budget_mib=budget, counts=Counter(),
                         planned_supported_dds_bytes=0, installed_dds_bytes=0,
                         visual_classification='pending', texture_ids=[])
              for name, budget in BUDGET_MIB.items()}
    for row in worklist['textures']:
        group = groups[route(row['archives'])]
        group['texture_ids'].append(row['id'])
        group['counts'][row['status']] += 1
        group['planned_supported_dds_bytes'] += row['estimated_dds_bytes'] or 0
        if row['status'] == 'installed':
            group['installed_dds_bytes'] += row['estimated_dds_bytes']
    formats = {}
    research_path = root / 'format-research.json'
    research = json.loads(research_path.read_text(encoding='utf-8')) if research_path.exists() else {}
    decoded = {row['sha256']: row for row in research.get('decoded_payloads', [])}
    for kind in sorted({t['archive_format'] for t in catalog['textures']}):
        rows = [t for t in catalog['textures'] if t['archive_format'] == kind]
        matches = Counter()
        for row in rows:
            if 'key' not in row:
                hypotheses = size_hypotheses(row['width'], row['height'], row['bytes'])
                matches.update(hypotheses or ['no uncompressed byte-count match'])
        formats[str(kind)] = dict(payloads=len(rows),
            supported_payloads=sum('key' in t for t in rows),
            byte_count_hypotheses=dict(matches),
            action='art production' if all('key' in t for t in rows)
                   else 'verify encoding, channel order, layout and runtime identity before enabling')
        if rows and all(t['sha256'] in decoded for t in rows):
            formats[str(kind)].update(
                decoded_payloads=len(rows),
                verified_xbox_formats=sorted({decoded[t['sha256']]['xbox_format'] for t in rows}),
                proof='format-research.json: pinned executable switch verified; all original payload hashes checked',
                action=('art production; uncompressed runtime scene validation pending'
                        if all('key' in t for t in rows) else
                        'decoded offline; integrate runtime replacement support and validate identity/alpha handling'))
    totals = Counter()
    for row in archive_rows:
        totals.update({k: v for k, v in row.items() if isinstance(v, int)})
    embedded_path=root/'embedded-decode-research.json'
    embedded=json.loads(embedded_path.read_text(encoding='utf-8')) if embedded_path.exists() else {}
    embedded_summary={k:embedded[k] for k in ('unique_decoded_keys','new_keys_pending_catalog','potential_total_unique_keys','next_step') if k in embedded}
    return dict(schema=1, complete=False, scope=catalog['scope'],
        classification_note='Archive routing is provisional; effects, props and UI inside level archives need visual review.',
        totals=dict(totals), archive_count=len(archive_rows),
        catalog_payloads=len(catalog['textures']),
        unique_supported_keys=worklist['unique_supported_keys'],
        counts=worklist['counts'], formats=formats,
        embedded_research=embedded_summary,
        xbe_section_research=xbe_summary,
        budget_mib=sum(BUDGET_MIB.values()), groups=groups,
        pack_bytes=worklist['pack_bytes'],
        texture_workspace_allocated_bytes=worklist['texture_workspace_allocated_bytes'],
        disk_free_bytes=worklist['disk_free_bytes'],
        coverage_gaps=['Unclassified archive entries have not been proven texture-free.',
                       'Embedded texture discovery is incomplete.',
                       'Five XBE error screens are verified; other executable encodings, videos and procedural surfaces remain open.',
                       'Uncompressed identities are verified offline; scene validation remains pending.',
                       'Installed textures still require offline visual sign-off and final scene checks.'],
        playtest_gate=dict(minimum_offline_approved_fraction=0.9,
                          all_asset_categories_reviewed=False, ready=False,
                          note='Installed count alone does not satisfy offline visual approval.'),
        archives=archive_rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, default=Path('private/game'))
    parser.add_argument('--root', type=Path, default=Path('private/texture-remaster'))
    args = parser.parse_args()
    report = build(args.game, args.root)
    destination = args.root / 'inventory.json'
    destination.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k not in ('groups', 'archives')}, indent=2))
    for name, group in report['groups'].items():
        print(name, json.dumps({k: v for k, v in group.items() if k != 'texture_ids'}))


if __name__ == '__main__':
    main()
