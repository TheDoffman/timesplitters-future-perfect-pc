"""Offline decoded sample metrics. Flags are review candidates, not defects."""
import ctypes
import hashlib

import numpy as np


class Decoder:
    def __init__(self, library):
        self.library = ctypes.CDLL(str(library.resolve()))
        self.decode = self.library.audio_audit_decode
        self.decode.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint,
                                ctypes.c_void_p, ctypes.c_size_t]
        self.decode.restype = ctypes.c_int

    def pcm(self, data, channels):
        if channels not in (1, 2) or len(data) % (36 * channels):
            raise ValueError('Incomplete Xbox ADPCM block')
        samples = len(data) // (36 * channels) * 64 * channels
        pcm = np.empty(samples, dtype=np.int16)
        source = ctypes.create_string_buffer(data)
        if not self.decode(source, len(data), channels, pcm.ctypes.data, samples):
            raise ValueError('Production decoder rejected input')
        return pcm.reshape(-1, channels)


def metrics(pcm):
    if pcm.ndim != 2 or pcm.shape[1] not in (1, 2) or not len(pcm):
        raise ValueError('Expected nonempty mono/stereo frames')
    signal = pcm.astype(np.float64) / 32768
    peak = float(np.max(np.abs(signal)))
    rms = np.sqrt(np.mean(signal * signal, axis=0))
    differences = np.abs(np.diff(signal, axis=0))
    return dict(frames=len(pcm), channels=pcm.shape[1], peak=peak,
                rms_dbfs=[float(20 * np.log10(v)) if v > 0 else None for v in rms],
                dc_offset=np.mean(signal, axis=0).tolist(),
                full_scale_samples=int(np.sum((pcm == 32767) | (pcm == -32768))),
                silent_frames=int(np.sum(np.all(pcm == 0, axis=1))),
                seam_step=np.abs(signal[0] - signal[-1]).tolist(),
                max_adjacent_step=np.max(differences, axis=0).tolist() if len(differences) else [0.] * pcm.shape[1],
                pcm_sha256=hashlib.sha256(pcm.tobytes()).hexdigest(),
                meaning='unweighted sample metrics; full scale/seam size alone do not establish distortion or a bad loop')


def measure_payload(stream, row, decoder, cache):
    if row['compressed']:
        return dict(status='compressed_payload_not_decoded')
    stream.seek(row['offset'])
    data = stream.read(row['stored_bytes'])
    if len(data) != row['stored_bytes']:
        raise ValueError('Truncated audio payload')
    digest = hashlib.sha256(data).hexdigest()
    channels = row['channels']
    block = 36 * channels
    complete = len(data) // block * block
    key = (digest, channels)
    if key not in cache:
        if not complete:
            result = dict(status='no_complete_adpcm_blocks')
        else:
            pcm = decoder.pcm(data[:complete], channels)
            result = metrics(pcm)
            headers = np.frombuffer(data[:complete], dtype=np.uint8).reshape(-1, block)
            result.update(status='decoded_production',
                          clamped_step_headers=sum(int(np.sum(headers[:, c * 4 + 2] > 88)) for c in range(channels)),
                          nonzero_reserved_headers=sum(int(np.sum(headers[:, c * 4 + 3] != 0)) for c in range(channels)))
        cache[key] = result
    result = dict(cache[key], source_sha256=digest, trailing_bytes=len(data) - complete)
    if 'frames' in result:
        result['durations_at_base_frequency'] = [dict(hz=rate, seconds=result['frames'] / rate)
                                                for rate in row['base_frequencies_hz']]
    return result
