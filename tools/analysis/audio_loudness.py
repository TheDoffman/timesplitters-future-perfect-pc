"""Measure original bank audio with FFmpeg's BS.1770 loudness/true-peak meter.

Only input measurements are kept; processed output goes to the null muxer.
Raw Xbox blocks travel through stdin in a temporary in-memory WAVE envelope.
No extracted/replacement audio files are written. FFmpeg and the production
decoder have small IMA rounding differences; these are metering references,
not bit-exact decoder equivalence evidence. Short signals may not yield LUFS.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess

from audio_audit import SUPPORTED_XBE, digest


def wave_envelope(data, channels, rate):
    if channels not in (1, 2) or not 1000 <= rate <= 200000 or len(data) % (36 * channels):
        raise ValueError('Invalid Xbox ADPCM WAVE input')
    fmt = struct.pack('<HHIIHHHH', 0x69, channels, rate, rate * 36 * channels // 64,
                      36 * channels, 4, 2, 64)
    body = b'WAVEfmt ' + struct.pack('<I', len(fmt)) + fmt
    body += b'data' + struct.pack('<I', len(data)) + data
    return b'RIFF' + struct.pack('<I', len(body)) + body


def finite(value):
    number = float(value)
    return number if math.isfinite(number) else None


def measure(ffmpeg, data, channels, rate):
    result = subprocess.run([str(ffmpeg), '-nostdin', '-hide_banner', '-nostats',
        '-f', 'wav', '-i', 'pipe:0', '-af', 'loudnorm=I=-23:TP=-1:LRA=7:print_format=json',
        '-f', 'null', '-'], input=wave_envelope(data, channels, rate), capture_output=True,
        timeout=60)
    log = result.stderr.decode('utf-8', errors='replace')
    if result.returncode:
        raise ValueError('FFmpeg measurement failed: ' + log[-2000:])
    start = log.rfind('{')
    if start < 0:
        raise ValueError('FFmpeg did not report input loudness')
    report, _ = json.JSONDecoder().raw_decode(log[start:])
    return dict(integrated_lufs=finite(report['input_i']),
                true_peak_dbtp=finite(report['input_tp']), loudness_range_lu=finite(report['input_lra']),
                gating_threshold_lufs=finite(report['input_thresh']),
                status='measured' if finite(report['input_i']) is not None else 'below_gate_or_too_short')


def run(game, inventory, ffmpeg, scope, progress=print):
    game = game.resolve()
    if inventory['xbe_sha256'] != SUPPORTED_XBE or digest(game / 'default.xbe') != SUPPORTED_XBE:
        raise ValueError('Unsupported or changed game executable')
    version = subprocess.check_output([str(ffmpeg), '-version'], text=True, timeout=10).splitlines()[0]
    cache, records = {}, []
    for bank in inventory['archives']:
        selected = [row for row in bank.get('payloads', []) if 'measurement' in row and
                    (scope == 'all' or (scope == 'music' and row['name'].startswith('music/')) or
                     (scope == 'loops' and row['looping']))]
        if not selected:
            continue
        path = (game / bank['path']).resolve()
        if not path.is_relative_to(game) or digest(path) != bank['sha256']:
            raise ValueError('Archive is outside the game root or differs from inventory')
        with path.open('rb') as stream:
            for row in selected:
                if row['compressed'] or row['measurement']['trailing_bytes']:
                    raise ValueError('Payload is not a verified complete raw ADPCM stream')
                stream.seek(row['offset'])
                data = stream.read(row['stored_bytes'])
                source_hash = hashlib.sha256(data).hexdigest()
                if source_hash != row['measurement']['source_sha256']:
                    raise ValueError('Audio payload differs from decoded inventory')
                for rate in row['base_frequencies_hz']:
                    key = (source_hash, row['channels'], rate)
                    if key not in cache:
                        cache[key] = measure(ffmpeg, data, row['channels'], rate)
                    records.append(dict(archive=bank['path'], crc=row['crc'], name=row['name'],
                                        source_sha256=source_hash, channels=row['channels'], rate=rate,
                                        **cache[key]))
                if progress and len(records) % 100 == 0:
                    progress(f'Metered {len(records)} payload/rate records ({len(cache)} unique)', flush=True)
    return dict(schema=1, complete_for_selected_scope=True, scope=scope, ffmpeg_version=version,
                ffmpeg_sha256=digest(ffmpeg), xbe_sha256=SUPPORTED_XBE,
                unique_payload_layout_rates=len(cache), records=records,
                limits=['Original base frequency; dynamic pitch and in-game gains are excluded.',
                        'Not a master-mix, headroom, listening, or normalization recommendation.',
                        'Short/quiet sounds can lack gated integrated loudness; null is not zero LUFS.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--inventory', type=Path, required=True)
    parser.add_argument('--ffmpeg', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--scope', choices=('all', 'music', 'loops'), default='all')
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(args.game.resolve()):
        parser.error('Output must be outside original game data')
    inventory = json.loads(args.inventory.read_text(encoding='utf-8'))
    report = run(args.game, inventory, args.ffmpeg, args.scope)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(args.output.suffix + '.tmp')
    temporary.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    temporary.replace(args.output)
    print(f'Wrote private loudness report: {args.output}')


if __name__ == '__main__':
    main()
