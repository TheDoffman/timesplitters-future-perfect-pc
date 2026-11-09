"""Read-only survey of unidentified standalone P5CK payloads.

Header compatibility is a candidate filter, never proof of audio identity or
playback use. Embedded/other formats remain outside this survey's evidence.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import struct

from audio_audit import SUPPORTED_XBE, digest, entries


def compatible_layout(stream, offset, length, channels):
    """Check every original Xbox ADPCM header, including stereo's adjacent headers."""
    if channels not in (1, 2):
        raise ValueError('Expected mono or stereo candidate')
    block = 36 * channels
    if not length or length % block:
        return None
    stream.seek(offset)
    remaining, nonzero = length, False
    while remaining:
        chunk = stream.read(min(remaining, block * 4096))
        if not chunk or len(chunk) % block:
            raise ValueError('Truncated candidate payload')
        remaining -= len(chunk)
        nonzero = nonzero or any(chunk)
        for at in range(0, len(chunk), block):
            for channel in range(channels):
                header = at + channel * 4
                if chunk[header + 2] > 88 or chunk[header + 3]:
                    return None
    return dict(channels=channels, blocks=length // block, all_zero=not nonzero)


def signatures(prefix, length):
    found = []
    if (len(prefix) >= 12 and prefix[:4] == b'RIFF' and prefix[8:12] == b'WAVE'
            and 4 <= struct.unpack_from('<I', prefix, 4)[0] <= length - 8):
        found.append('riff_wave_header')
    if len(prefix) >= 48 and prefix[:4] == b'VAGp':
        size, rate = struct.unpack_from('>II', prefix, 12)
        if 0 < size <= length - 48 and 1000 <= rate <= 200000:
            found.append('vag_header')
    if len(prefix) >= 27 and prefix[:5] == b'OggS\0':
        found.append('ogg_container_header')
    if len(prefix) >= 36 and prefix[12:16] == b'xobX':
        found.append('xmv_container_magic')
    return found


def payload_digest(stream, offset, length):
    stream.seek(offset)
    result = hashlib.sha256()
    while length:
        chunk = stream.read(min(length, 1024 * 1024))
        if not chunk:
            raise ValueError('Truncated candidate payload')
        result.update(chunk)
        length -= len(chunk)
    return result.hexdigest()


def survey(game, inventory):
    """Reuse CRC exclusions only from a hash-checked, decoded/copy-verified census."""
    copies = inventory.get('known_audio_copies', {})
    if copies.get('status') != 'verified_known_audio_copies' or copies.get('unresolved'):
        raise ValueError('Use a decoded census with all known copies verified')
    game = Path(game).resolve()
    known_crc, known_content = defaultdict(set), defaultdict(dict)
    for bank in inventory['archives']:
        for row in bank.get('payloads', []):
            measurement = row.get('measurement', {})
            if 'source_sha256' not in measurement:
                continue
            known_crc[row['crc']].add(measurement['source_sha256'])
            key = (row.get('name'), row.get('channels'))
            known_content[measurement['source_sha256']][key] = dict(
                name=row.get('name'), channels=row.get('channels'), base_frequencies_hz=row.get('base_frequencies_hz', []))
    if not known_crc:
        raise ValueError('Decoded audio mappings are absent')
    counts, candidates = Counter(), []
    for bank in inventory['archives']:
        path = (game / bank['path']).resolve()
        if not path.is_relative_to(game):
            raise ValueError('Archive path escapes original data root')
        if digest(path) != bank['sha256']:
            raise ValueError('Archive changed since census: ' + bank['path'])
        counts['archives_hash_verified'] += 1
        if 'payloads' in bank:
            continue
        with path.open('rb') as stream:
            rows = entries(stream, path.stat().st_size)
            for index, (crc, offset, length, compressed) in enumerate(rows):
                counts['other_entries'] += 1
                key = f'{crc:08x}'
                if key in known_crc:
                    if compressed or payload_digest(stream, offset, length) not in known_crc[key]:
                        raise ValueError('Known-CRC copy no longer matches decoded content')
                    counts['previously_verified_known_crc_entries'] += 1
                    continue
                counts['unidentified_entries'] += 1
                if compressed:
                    counts['compressed_not_surveyed'] += 1
                    continue
                counts['uncompressed_entries_surveyed'] += 1
                stream.seek(offset)
                prefix = stream.read(min(length, 64))
                tagged = signatures(prefix, length)
                layouts = []
                for channels in (1, 2):
                    layout = compatible_layout(stream, offset, length, channels)
                    if layout is not None:
                        layouts.append(layout)
                if not tagged and not layouts:
                    continue
                sha = payload_digest(stream, offset, length)
                matches = list(known_content.get(sha, {}).values())
                counts['candidate_entries'] += 1
                counts['header_compatible_entries'] += bool(layouts)
                counts['signature_entries'] += bool(tagged)
                counts['identical_to_known_content'] += bool(matches)
                counts['all_zero_compatible_entries'] += bool(layouts and all(x['all_zero'] for x in layouts))
                candidates.append(dict(archive=bank['path'], index=index, crc=key, offset=offset, bytes=length,
                    sha256=sha, signatures=tagged, compatible_layouts=layouts, identical_content=matches,
                    status='candidate_only; identity, channel ownership and playback use not inferred'))
    return dict(schema=1, complete=False, counts=dict(counts), candidates=candidates,
        limits=['Standalone uncompressed entries only; compressed/embedded/other formats are not ruled out.',
                'ADPCM-compatible headers and container signatures do not establish audio identity.',
                'All-zero data and identical bytes can occur in non-audio resources; playback use still needs evidence.',
                'This survey does not modify or decode source payloads, relabel the original inventory, or select restoration.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', required=True, type=Path)
    parser.add_argument('--inventory', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    game, output = args.game.resolve(), args.output.resolve()
    if output.is_relative_to(game) or output.exists():
        parser.error('Use a new private output outside original game data')
    if digest(game / 'default.xbe') != SUPPORTED_XBE:
        parser.error('Unsupported executable')
    inventory = json.loads(args.inventory.read_text(encoding='utf-8'))
    if inventory.get('xbe_sha256') != SUPPORTED_XBE:
        parser.error('Inventory belongs to a different executable')
    report = survey(game, inventory)
    report.update(xbe_sha256=SUPPORTED_XBE, inventory_sha256=digest(args.inventory))
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    print(json.dumps(report['counts'], sort_keys=True))


if __name__ == '__main__':
    main()
