"""Build a small hybrid production queue; never generate or install artwork.

Run texture-worklist.py first for the authoritative pack audit. Classification
is provisional: a batch candidate still needs original-image/alpha inspection.
Unknown assets are triage work, never silently approved for bulk enhancement.
"""
import argparse
from collections import Counter
import json
from pathlib import Path

PACK_BUDGET = 6144 * 1024 * 1024
# Inventory labels include mixed uses such as prop_or_interface and
# effect_or_reflection. Inspect their semantic parts rather than maintaining
# a partial list that silently sends newer sensitive labels to ordinary review.
SPECIAL_PARTS = {'effect', 'interface', 'reflection', 'signage', 'utility',
                 'artwork', 'identity', 'uncertain', 'alpha', 'cutout',
                 'foliage', 'sky', 'font', 'logo', 'decal'}
MATERIAL_CATEGORIES = {'environment', 'environment_material'}


def lane(texture, classification):
    status = texture['status']
    if status == 'retained_original':
        return 'retained_original'
    if status == 'installed':
        return 'installed'
    if status == 'draft_requires_review':
        return 'correction'
    if status != 'needs_art':
        return 'format_research'
    if not classification:
        return 'visual_triage'
    category = classification['visual_category']
    if SPECIAL_PARTS.intersection(category.split('_')):
        return 'specialist_review'
    if category in MATERIAL_CATEGORIES:
        return 'batch_candidate'
    return 'individual_review'


def family(description):
    # Sorting hint only; these words cannot authorize an edit or establish usage.
    text = description.lower()
    for name, words in [('wood', ('wood', 'timber', 'plank')),
                        ('stone', ('stone', 'brick', 'concrete', 'rock')),
                        ('metal', ('metal', 'steel', 'plate', 'circuit', 'rivet'))]:
        if any(word in text for word in words):
            return name
    return 'other'


def build(worklist, classification, batch_size=8):
    if not 1 <= batch_size <= 16:
        raise ValueError('Batch size must be 1..16')
    if not 0 <= worklist['pack_bytes'] <= PACK_BUDGET:
        raise ValueError('Active pack is outside the fixed 6 GiB budget')
    records = {r['key']: r for r in classification['records']}
    if len(records) != len(classification['records']):
        raise ValueError('Duplicate visual classifications')
    rows = []
    seen = set()
    for texture in worklist['textures']:
        key = texture['id']
        if key in seen:
            raise ValueError('Duplicate worklist key: ' + key)
        seen.add(key)
        record = records.get(key)
        route = lane(texture, record)
        description = record['description'] if record else ''
        rows.append(dict(key=key, lane=route, family=family(description),
                         occurrences=texture.get('occurrences', 0),
                         original_size=texture['original_size'],
                         description=description))
    rows.sort(key=lambda r: (r['lane'], r['family'], -r['occurrences'], r['key']))
    batches = []
    candidates = [r for r in rows if r['lane'] == 'batch_candidate']
    for material in sorted({r['family'] for r in candidates}):
        group = [r['key'] for r in candidates if r['family'] == material]
        for start in range(0, len(group), batch_size):
            batches.append(dict(family=material, keys=group[start:start + batch_size],
                                status='inspect originals and alpha before authoring'))
    return dict(schema=1, strategy='hybrid', complete=False,
                pack_budget_bytes=PACK_BUDGET,
                pack_bytes=worklist['pack_bytes'],
                pack_headroom_bytes=PACK_BUDGET - worklist['pack_bytes'],
                counts=dict(Counter(r['lane'] for r in rows)),
                batch_size=batch_size, candidate_batches=batches, textures=rows,
                notes=['Routing is not visual approval or a completion count.',
                       'Use built-in image generation; retain original channel semantics.',
                       'Use modest dimensions for secondary assets; reserve up to 4K for prominent surfaces.',
                       'Defer failed drafts to correction batches; do not repeatedly stall production.',
                       'Playtesting requires 90% of audited scope and all categories reviewed.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path('private/texture-remaster'))
    parser.add_argument('--batch-size', type=int, default=8)
    args = parser.parse_args()
    read = lambda name: json.loads((args.root / name).read_text(encoding='utf-8'))
    report = build(read('worklist.json'), read('visual-classification.json'), args.batch_size)
    (args.root / 'hybrid-plan.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k != 'textures'}, indent=2))


if __name__ == '__main__':
    main()
