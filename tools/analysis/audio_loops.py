"""Read-only supported-US authored-loop audit; reports stay private.

US26E80 resolves eight name/time records at 4B90B0 (count 4B9110).
US274A0/275AA converts their seconds using the current playback frequency,
truncates to 64-sample blocks and calls IDirectSoundBuffer_SetLoopRegion.
This report evaluates original base rates, not dynamic pitch or live API state.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import struct

from audio_bank import XBEParser
from audio_audit import SUPPORTED_XBE, digest


def float32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


def loop_metadata(image):
    count, = struct.unpack('<I', image.at(0x4B9110, 4))
    if not 0 < count <= 256:
        raise ValueError('Invalid authored-loop record count')
    rows, seen = [], set()
    for index in range(count):
        pointer, start, end = struct.unpack('<Iff', image.at(0x4B90B0 + index * 12, 12))
        name = image.string(pointer)
        if (not name or len(name) > 192 or any(ord(c) < 32 or ord(c) > 126 for c in name)
                or name in seen or not math.isfinite(start) or not math.isfinite(end)
                or start < 0 or end <= start):
            raise ValueError('Invalid authored-loop name or time range')
        seen.add(name)
        rows.append(dict(name=name, start_seconds=start, end_seconds=end))
    return rows


def encoded_region(row, rate):
    """Positive SSE float multiply/CVTTSS2SI, then 64 samples -> 36 bytes."""
    if not isinstance(rate, int) or not 188 <= rate <= 191983:
        raise ValueError('Frequency outside original caller bounds')
    result = []
    for key in ('start_seconds', 'end_seconds'):
        try:
            samples = float32(float32(row[key]) * float32(rate))
        except OverflowError as error:
            raise ValueError('Loop time overflows original single precision') from error
        if not math.isfinite(samples) or not 0 <= samples < 2147483648:
            raise ValueError('Loop time cannot be converted by the original caller')
        result.append(int(samples) // 64 * 36)
    return tuple(result)


def map_regions(records, inventory):
    result = []
    for record in records:
        matches = []
        for bank in inventory['archives']:
            for payload in bank.get('payloads', []):
                path = PurePosixPath(payload.get('name', ''))
                if path.suffix == '.xbs' and path.stem == record['name']:
                    matches.append((bank, payload))
        entry = dict(record, matches=[])
        for bank, payload in matches:
            rates = []
            for rate in payload.get('base_frequencies_hz', []):
                start, end = encoded_region(record, rate)
                valid = payload.get('channels') == 1 and start < end <= payload['stored_bytes']
                rates.append(dict(hz=rate, start_byte=start, end_byte=end,
                                  start_frame=start // 36 * 64, end_frame=end // 36 * 64,
                                  status='within_source' if valid else 'outside_verified_source_layout'))
            entry['matches'].append(dict(bank=bank['path'], name=payload['name'],
                                         source_sha256=payload.get('measurement', {}).get('source_sha256'),
                                         rates=rates))
        entry['status'] = 'mapped' if matches else 'not_found_in_inventory'
        result.append(entry)
    return result


def measure_regions(rows, game, inventory, decoder):
    """Verify the existing census hashes before measuring actual loop boundaries."""
    banks = {bank['path']: bank for bank in inventory['archives']}
    for row in rows:
        for match in row['matches']:
            bank = banks[match['bank']]
            payload, = [p for p in bank['payloads'] if p.get('name') == match['name']]
            source = (game / bank['path']).resolve()
            if not source.is_relative_to(game) or payload['compressed']:
                raise ValueError('Unverified archive path or payload compression')
            if not 0 < payload['stored_bytes'] <= 64 * 1024 * 1024:
                raise ValueError('Unbounded loop payload')
            with source.open('rb') as stream:
                stream.seek(payload['offset'])
                data = stream.read(payload['stored_bytes'])
            if len(data) != payload['stored_bytes'] or hashlib.sha256(data).hexdigest() != match['source_sha256']:
                raise ValueError('Source payload differs from the measured inventory')
            pcm = decoder.pcm(data, payload['channels'])
            if hashlib.sha256(pcm.tobytes()).hexdigest() != payload['measurement']['pcm_sha256']:
                raise ValueError('Production PCM differs from the measured inventory')
            for rate in match['rates']:
                if rate['status'] != 'within_source':
                    continue
                start, end = rate['start_frame'], rate['end_frame']
                if not 0 <= start < end <= len(pcm):
                    raise ValueError('Decoded loop extent exceeds PCM')
                rate['seam_step'] = [abs(int(a) - int(b)) / 32768
                                     for a, b in zip(pcm[start], pcm[end - 1])]
                rate['loop_seconds'] = (end - start) / rate['hz']
                rate['intro_frames'] = start


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', required=True, type=Path)
    parser.add_argument('--inventory', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--decoder-library', type=Path)
    args = parser.parse_args()
    game, output = args.game.resolve(), args.output.resolve()
    if output.is_relative_to(game) or output.exists():
        parser.error('Use a new private output outside original game data')
    xbe = game / 'default.xbe'
    if digest(xbe) != SUPPORTED_XBE:
        parser.error('Unsupported executable')
    inventory = json.loads(args.inventory.read_text(encoding='utf-8'))
    if inventory.get('xbe_sha256') != SUPPORTED_XBE:
        parser.error('Inventory belongs to a different executable')
    image = XBEParser(xbe)
    image.parse()
    rows = map_regions(loop_metadata(image), inventory)
    if args.decoder_library:
        from audio_measurements import Decoder
        measure_regions(rows, game, inventory, Decoder(args.decoder_library))
    report = dict(schema=1, complete=False, xbe_sha256=SUPPORTED_XBE,
                  inventory_sha256=digest(args.inventory),
                  decoder_library_sha256=digest(args.decoder_library) if args.decoder_library else None,
                  records=rows, limits=[
                      'Only the original named-loop table is covered, not every possible runtime loop.',
                      'Unmatched definitions remain unclassified; absence from known banks does not prove unused.',
                      'Byte ranges use original base frequency; dynamic pitch and live HLE loop handling are separate checks.',
                      'Boundary steps are review metrics, not proof of an audible click or reason to rewrite assets.'])
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    print(f'Wrote private loop audit: {output}')


if __name__ == '__main__':
    main()
