"""Read-only audio container audit; never extracts or modifies game assets.

P5CK field layout: OpenRadical/tspak, tspak.h (Entry5).
Bank filename lists are observed structure, not a verified codec/name mapping.
Raw VAG/MIB names do not prove PlayStation encodings in the Xbox release.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct

from audio_bank import map_bank, map_music, xbe_metadata

SUPPORTED_XBE = '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
MAX_INDEX = 16 * 1024 * 1024
MAX_BANK = 4 * 1024 * 1024


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def entries(stream, size):
    header = stream.read(12)
    if len(header) != 12 or header[:4] != b'P5CK':
        raise ValueError('Expected P5CK header')
    offset, length = struct.unpack_from('<II', header, 4)
    if offset < 12 or length % 16 or length > MAX_INDEX or offset + length > size:
        raise ValueError('Invalid P5CK index extent')
    stream.seek(offset)
    table = stream.read(length)
    if len(table) != length:
        raise ValueError('Truncated P5CK index')
    rows = []
    for crc, start, decoded_size, compressed in struct.iter_unpack('<4I', table):
        stored = compressed or decoded_size
        if start < 12 or start + stored > size:
            raise ValueError('P5CK payload outside archive')
        if stored and start < offset + length and start + stored > offset:
            raise ValueError('P5CK payload overlaps index')
        rows.append((crc, start, decoded_size, compressed))
    return rows


def filename_list(data):
    """Identify only a bounded leading u16 + u8-length ASCII filename list."""
    if len(data) < 3:
        raise ValueError('Truncated filename list')
    count = struct.unpack_from('<H', data)[0]
    if not 1 <= count <= 20000:
        raise ValueError('Invalid filename count')
    offset, names = 2, []
    for _ in range(count):
        if offset >= len(data):
            raise ValueError('Truncated filename length')
        length = data[offset]
        offset += 1
        if not length or offset + length > len(data):
            raise ValueError('Truncated or empty filename')
        raw = data[offset:offset + length]
        if any(c < 32 or c > 126 for c in raw):
            raise ValueError('Non-ASCII filename')
        name = raw.decode('ascii')
        if Path(name).suffix.lower() not in ('.vag', '.mib', '.wav', '.wma'):
            raise ValueError('Unrecognized filename extension')
        names.append(name)
        offset += length
    return dict(count=count, bytes_consumed=offset, remaining_bytes=len(data) - offset,
                extensions=dict(sorted(Counter(Path(n).suffix.lower() for n in names).items())),
                names=names, payload_mapping_verified=False,
                status='observed_filename_list; codec and following records unverified')


def audit_archive(path, game, include_payloads):
    size = path.stat().st_size
    result = dict(path=path.relative_to(game).as_posix(), bytes=size, sha256=digest(path))
    with path.open('rb') as stream:
        rows = entries(stream, size)
        result.update(entries=len(rows), compressed_entries=sum(bool(r[3]) for r in rows),
                      stored_payload_bytes=sum(r[3] or r[2] for r in rows))
        if not include_payloads:
            result['audio_status'] = 'not_classified; archive index only'
            return result
        result['audio_status'] = 'candidate_by_archive_name; encoding unverified'
        payloads, prefixes = [], Counter()
        for index, (crc, offset, length, compressed) in enumerate(rows):
            stored = compressed or length
            stream.seek(offset)
            head = stream.read(min(stored, 32))
            prefixes[head[:4].hex()] += 1
            record = dict(index=index, crc=f'{crc:08x}', offset=offset,
                          decoded_bytes=length, stored_bytes=stored, compressed=bool(compressed),
                          prefix_hex=head.hex(), codec='unverified')
            if index == 0 and not compressed and stored <= MAX_BANK:
                stream.seek(offset)
                try:
                    result['filename_list'] = filename_list(stream.read(stored))
                except ValueError as error:
                    result['filename_list_status'] = str(error)
            payloads.append(record)
        result.update(payloads=payloads, prefix_counts=dict(sorted(prefixes.items())))
    return result


def known_audio_copies(game, archives):
    """Verify level/other copies of mapped audio by CRC plus complete SHA-256."""
    from collections import defaultdict
    known = defaultdict(set)
    for bank in archives:
        for row in bank.get('payloads', []):
            if 'measurement' in row and 'source_sha256' in row['measurement']:
                known[row['crc']].add(row['measurement']['source_sha256'])
    if not known:
        return dict(status='not_measured; requires production decode inventory')
    counts, matches, unresolved = Counter(), [], []
    for bank in archives:
        if 'payloads' in bank:
            continue
        found = 0
        with (game / bank['path']).open('rb') as stream:
            rows = entries(stream, bank['bytes'])
            counts['other_entries'] += len(rows)
            for crc, offset, length, compressed in rows:
                key = f'{crc:08x}'
                if key not in known:
                    continue
                found += 1
                counts['known_audio_entries'] += 1
                stream.seek(offset)
                raw = stream.read(compressed or length)
                if not compressed and hashlib.sha256(raw).hexdigest() in known[key]:
                    counts['identical_to_bank'] += 1
                else:
                    unresolved.append(dict(archive=bank['path'], crc=key,
                                           status='compressed_or_different_copy'))
        if found:
            matches.append(dict(archive=bank['path'], known_audio_entries=found))
    return dict(status='verified_known_audio_copies', counts=dict(counts), archives=matches,
                unresolved=unresolved, limits='Unknown paths/embedded audio remain unclassified.')


def audit(game, progress=None, decoder_library=None):
    game = game.resolve()
    xbe = game / 'default.xbe'
    xbe_hash = digest(xbe)
    if xbe_hash != SUPPORTED_XBE:
        raise ValueError('Unsupported default.xbe SHA-256; expected the US Xbox release')
    frequencies, music = xbe_metadata(xbe)
    decoder, cache = None, {}
    if decoder_library:
        from audio_measurements import Decoder
        decoder = Decoder(decoder_library)
    paths = sorted((game / 'pak').rglob('*.pak'))
    if not paths:
        raise ValueError('No P5CK archives found')
    archives = []
    for index, path in enumerate(paths):
        candidate = path.stem.lower().startswith('sounds_') or path.stem.lower() == 'musicts'
        bank = audit_archive(path, game, candidate)
        if candidate:
            with path.open('rb') as stream:
                if path.stem.lower() == 'musicts':
                    map_music(bank, music)
                else:
                    metadata = bank['payloads'][0]
                    if metadata['compressed'] or metadata['stored_bytes'] > MAX_BANK:
                        raise ValueError('Unverified sound metadata encoding')
                    stream.seek(metadata['offset'])
                    map_bank(bank, stream.read(metadata['stored_bytes']), frequencies)
                if decoder:
                    from audio_measurements import measure_payload
                    for row in bank['payloads']:
                        if 'channels' in row:
                            row['measurement'] = measure_payload(stream, row, decoder, cache)
        archives.append(bank)
        if progress and ((index + 1) % 25 == 0 or index + 1 == len(paths)):
            progress(f'Indexed and hashed {index + 1}/{len(paths)} archives', flush=True)
    movies = [dict(path=p.relative_to(game).as_posix(), bytes=p.stat().st_size,
                   sha256=digest(p), audio_status='not_decoded')
              for p in sorted((game / 'xmv').glob('*.xmv'))]
    return dict(schema=2, xbe_sha256=xbe_hash, complete=False,
                decoder_library_sha256=digest(decoder_library) if decoder_library else None,
                unique_decoded_payload_layouts=len(cache),
                known_audio_copies=known_audio_copies(game, archives),
                method='Full P5CK index/bounds and SHA-256 census; original-loader sound/music mappings; optional production decode metrics; no extraction.',
                archive_count=len(archives), archives=archives, movies=movies,
                limits=['Other archives remain unclassified, not excluded from audio scope.',
                        'Sound frequency is original base playback frequency; dynamic pitch/randomization and explicit loop subregions remain outside this census.',
                        'Sample RMS is not perceptual/integrated loudness. Full-scale samples and seam steps require listening review.',
                        'Movies are hashed, not decoded here; host-output listening and rendered reference captures remain pending.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path,
                        help='Private JSON destination outside the original game-data folder')
    parser.add_argument('--decoder-library', type=Path,
                        help='Locally built audio_decode_probe linked with the production Xbox decoder')
    args = parser.parse_args()
    game, output = args.game.resolve(), args.output.resolve()
    if output.is_relative_to(game):
        parser.error('Output must be outside original game data')
    report = audit(game, print, args.decoder_library)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + '.tmp')
    temporary.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    temporary.replace(output)
    print(f'Wrote private inventory: {output}')


if __name__ == '__main__':
    main()
