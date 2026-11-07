"""Supported-US bank mapping, derived from original loader disassembly.

US27700 reads filename and event lists; US27290 converts platform extensions;
US350C70 resolves language paths; US1504F0 hashes names with standard CRC32.
US27FA0/US28528 derive frequency from the event's index and the original table.
US28890/US350C40 select music paths/rates. Do not infer codecs from extensions.
"""
from collections import defaultdict
from pathlib import Path
import struct
import sys
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'compiler'))
from xbe_image import XBEParser

EVENTS = 5703
LANGUAGES = {'e': 'sfx', 'f': 'sfx/french', 'g': 'sfx/german',
             'i': 'sfx/italian', 's': 'sfx/spanish'}


def parse_events(data, offset, names):
    records = []
    for index in range(EVENTS):
        if offset >= len(data):
            raise ValueError('Truncated sound event label length')
        length = data[offset]
        offset += 1
        if not length or offset + length + 10 > len(data):
            raise ValueError('Truncated sound event record')
        raw = data[offset:offset + length]
        if any(c < 32 or c > 126 for c in raw):
            raise ValueError('Invalid sound event label')
        offset += length
        asset, params, flags, extra = struct.unpack_from('<HHIH', data, offset)
        offset += 10
        if asset >= len(names) or params & 255 >= 35:
            raise ValueError('Sound event asset or frequency index out of range')
        records.append(dict(event=index, label=raw.decode('ascii'), asset=asset,
                            frequency_index=params & 255, volume_percent=params >> 8,
                            flags=f'{flags:08x}', channels=2 if flags & 0x200 else 1,
                            looping=bool(flags & 0x10), extra_word=extra))
    if offset != len(data):
        raise ValueError('Unparsed sound event metadata suffix')
    return records


def platform_path(name, language):
    prefix = LANGUAGES[language] if name.startswith('!') else 'sfx'
    name = name.removeprefix('!')
    ending = name[-3:]
    if ending.lower() == 'vag':
        name = name[:-3] + 'xbs'
    elif ending == 'MIB':
        name = name[:-3] + 'xbm'
    else:
        raise ValueError('Unverified platform filename conversion')
    return prefix + '/' + name


def xbe_metadata(path):
    image = XBEParser(path)
    image.parse()
    # These addresses are valid only after the caller verifies the supported hash.
    frequencies = struct.unpack('<35H', image.at(0x4ca9b8, 70))
    scale = struct.unpack('<f', image.at(0x4761d0, 4))[0]
    if scale != 1 / 4096:
        raise ValueError('Unexpected frequency scale')
    music_names = struct.unpack('<202I', image.at(0x535530, 808))
    music_rates = struct.unpack('<202I', image.at(0x535b80, 808))
    music = defaultdict(set)
    for pointer, rate in zip(music_names, music_rates):
        if pointer:
            name = image.string(pointer)
            if name.startswith('music/') and name.endswith('.xbm'):
                if rate not in (32000, 44100):
                    raise ValueError('Unexpected original music frequency')
                music[name].add(rate)
    return frequencies, {name: sorted(rates) for name, rates in music.items()}


def map_bank(bank, data, frequencies):
    observed = bank['filename_list']
    names = observed['names']
    events = parse_events(data, observed['bytes_consumed'], names)
    grouped = defaultdict(list)
    for event in events:
        event['base_frequency_hz'] = int(frequencies[event['frequency_index']] * 48000 / 4096)
        grouped[event['asset']].append(event)
    language = Path(bank['path']).stem.removeprefix('sounds_')
    payloads = {row['crc']: row for row in bank['payloads'][1:]}
    mapped = set()
    for index, name in enumerate(names):
        path = platform_path(name, language)
        crc = f'{zlib.crc32(path.encode("ascii")):08x}'
        if crc not in payloads or crc in mapped or not grouped[index]:
            raise ValueError('Missing/ambiguous sound payload mapping')
        row = payloads[crc]
        channels = {event['channels'] for event in grouped[index]}
        if len(channels) != 1:
            raise ValueError('Ambiguous sound channel layout')
        row.update(asset_index=index, name=path, events=grouped[index],
                   channels=channels.pop(), codec='xbox_adpcm_0x69',
                   base_frequencies_hz=sorted({event['base_frequency_hz'] for event in grouped[index]}),
                   looping=any(event['looping'] for event in grouped[index]),
                   loop_region='whole-source flag; explicit subregions not established')
        mapped.add(crc)
    if len(mapped) != len(payloads):
        raise ValueError('Unmapped sound payloads remain')
    observed['payload_mapping_verified'] = True
    bank['event_count'] = len(events)
    bank['audio_status'] = 'all sound payload CRCs mapped via original loader'


def map_music(bank, music):
    names = {f'{zlib.crc32(name.encode("ascii")):08x}': (name, rates)
             for name, rates in music.items()}
    if len(names) != len(music):
        raise ValueError('Ambiguous music CRC')
    for row in bank['payloads']:
        if row['crc'] not in names:
            raise ValueError('Unmapped music payload')
        name, rates = names[row['crc']]
        row.update(name=name, channels=2, base_frequencies_hz=rates,
                   codec='xbox_adpcm_0x69', looping=None,
                   loop_region='music playback policy; not inferred from filename')
    bank['audio_status'] = 'all music payload CRCs mapped via original music table'
