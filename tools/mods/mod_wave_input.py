"""Creator-side preflight for the independently enforced native PCM WAV contract."""
import struct

from mod_package import require

MAX_BYTES = 64 * 1024 * 1024
MAX_CHUNKS = 64
MAX_SECONDS = 600


def inspect_wave(data):
    require(len(data) <= MAX_BYTES, 'WAV exceeds64MiB')
    require(len(data) >= 12 and data[:4] == b'RIFF' and data[8:12] == b'WAVE'
            and struct.unpack_from('<I', data, 4)[0] == len(data)-8,
            'Expected an exact RIFF/WAVE container')
    at = 12
    chunks = 0
    format_fields = None
    pcm = None
    while at < len(data):
        chunks += 1
        require(chunks <= MAX_CHUNKS, 'WAV exceeds64 chunks')
        require(len(data)-at >= 8, 'Truncated WAV chunk')
        name, size = struct.unpack_from('<4sI', data, at)
        at += 8
        require(size <= len(data)-at, 'Truncated WAV chunk body')
        if name == b'fmt ':
            require(format_fields is None and size in (16, 18), 'Expected one PCM format chunk')
            require(size != 18 or struct.unpack_from('<H', data, at+16)[0] == 0,
                    'PCM format extension must be empty')
            format_fields = struct.unpack_from('<HHIIHH', data, at)
        elif name == b'data':
            require(format_fields is not None and pcm is None and size > 0,
                    'Expected one nonempty data chunk after fmt')
            pcm = (at, size)
        at += size
        if size & 1:
            require(at < len(data), 'Missing WAV chunk padding')
            at += 1
    require(format_fields is not None and pcm is not None, 'Missing WAV format or data')
    code, channels, rate, byte_rate, align, bits = format_fields
    require(code == 1 and channels in (1, 2) and 8000 <= rate <= 48000 and bits == 16
            and align == channels*2 and byte_rate == rate*align,
            'Expected PCM16 mono/stereo at8..48kHz with matching alignment/byte rate')
    require(pcm[1] % align == 0, 'WAV data must contain complete PCM frames')
    frames = pcm[1] // align
    require(frames <= rate*MAX_SECONDS, 'WAV exceeds600 seconds')
    return dict(rate=rate, channels=channels, frames=frames, pcm_bytes=pcm[1], pcm_offset=pcm[0])
