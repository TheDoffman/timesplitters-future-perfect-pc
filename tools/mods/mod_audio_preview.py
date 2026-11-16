"""Bounded PCM inspection only; no playback, file output or external decoder."""
from mod_package import require
from mod_wave_input import inspect_wave

MAX_PREVIEW_SECONDS = 2


def segment(data, offset):
    metadata = inspect_wave(data)
    require(type(offset) is int and 0 <= offset < metadata['frames'],
            'Invalid audio preview frame offset')
    end = min(metadata['frames'], offset + metadata['rate'] * MAX_PREVIEW_SECONDS)
    align = metadata['channels'] * 2
    start = metadata['pcm_offset'] + offset * align
    pcm = data[start:metadata['pcm_offset'] + end * align]
    return dict(rate=metadata['rate'], channels=metadata['channels'],
                frames=metadata['frames'], offset=offset, next_offset=end), pcm
