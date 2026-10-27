"""Search uncataloged P5CK entries in place; signature hits are research only."""

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
from collections import Counter
import hashlib
import importlib.util
import json
import mmap
from pathlib import Path
import re
import io

from PIL import Image, UnidentifiedImageError


def load(name):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


catalog_tool = load('texture-catalog')
nonpak = load('texture-nonpak-scan')
PATTERNS = {name: re.compile(re.escape(magic)) for name, magic in nonpak.SIGNATURES.items()}


class DecoderLimit(ValueError):
    pass


class EntryReader(io.RawIOBase):
    """Seekable bounded view: no whole-entry copies, and no cross-entry reads."""
    def __init__(self, data, start, end):
        self.data, self.start, self.end, self.pos = data, start, end, 0

    def readable(self):
        return True

    def seekable(self):
        return True

    def tell(self):
        return self.pos

    def seek(self, offset, whence=0):
        if whence not in (0, 1, 2):
            raise ValueError('Invalid seek origin')
        pos = offset + (self.pos if whence == 1 else self.end-self.start if whence == 2 else 0)
        if pos < 0:
            raise ValueError('Negative seek')
        self.pos = pos
        return pos

    def read(self, size=-1):
        remaining = max(0, self.end-self.start-self.pos)
        count = remaining if size is None or size < 0 else min(size, remaining)
        if count > 16*1024*1024:
            raise DecoderLimit('Decoder read exceeds memory limit')
        begin = self.start+self.pos
        self.pos += count
        return bytes(self.data[begin:begin+count])


def jpeg_candidate(data, start, end):
    """Decode within the entry, with a pixel cap; successful decode is not texture-use proof."""
    try:
        with EntryReader(data, start, end) as stream:
            with Image.open(stream, formats=['JPEG']) as picture:
                width, height = picture.size
                if width*height > 4096*4096:
                    return dict(status='jpeg_pixel_limit_unverified', width=width, height=height)
                picture.load()
                return dict(status='jpeg_decoded_candidate', width=width, height=height, mode=picture.mode)
    except (DecoderLimit, Image.DecompressionBombError) as error:
        return dict(status='jpeg_resource_limit_unverified', error_type=type(error).__name__)
    except (UnidentifiedImageError, OSError, ValueError, SyntaxError) as error:
        return dict(status='jpeg_decoder_rejected', error_type=type(error).__name__)


def scan_entry(data, start, end, sample_limit=32):
    """Never match across entry boundaries or borrow another entry's DDS bytes."""
    if not 0 <= start <= end <= len(data) or sample_limit < 0:
        raise ValueError('Invalid scan bounds or sample limit')
    signatures = {}
    for name, pattern in PATTERNS.items():
        count = 0
        samples = []
        statuses = Counter()
        for match in pattern.finditer(data, start, end):
            count += 1
            row = dict(relative_offset=match.start()-start, status='signature_only_not_verified')
            if name == 'dds':
                with memoryview(data)[match.start():end] as bounded:
                    row.update(nonpak.dds_candidate(bounded, 0))
            elif name == 'jpeg':
                row.update(jpeg_candidate(data, match.start(), end))
            statuses[row['status']] += 1
            if len(samples) < sample_limit:
                samples.append(row)
        if count:
            signatures[name] = dict(count=count, statuses=dict(statuses), samples=samples,
                                    samples_truncated=count > len(samples))
    return signatures


def scan(game, root, sample_limit=32, entry_limit=1000):
    if sample_limit < 0 or entry_limit < 0:
        raise ValueError('Report limits must be nonnegative')
    catalog_bytes = (root/'catalog.json').read_bytes()
    catalog = json.loads(catalog_bytes)
    baseline_bytes = (root/'embedded-scan.json').read_bytes()
    baseline = json.loads(baseline_bytes)
    expected = {a['archive']: a for a in baseline['archives']}
    paths = sorted((game/'pak').rglob('*.pak'))
    if {p.relative_to(game).as_posix() for p in paths} != set(expected):
        raise ValueError('Archive set differs from embedded-scan baseline')
    known = {(s['archive'], s['crc'], s['offset'], s['length'])
             for t in catalog['textures'] for s in t['sources']
             if not s.get('embedded') and s.get('source_kind') != 'xbe_section'}
    found = set()
    totals = Counter()
    signature_counts = Counter()
    dds_statuses = Counter()
    jpeg_statuses = Counter()
    archives = []
    hits = []
    for path in paths:
        rel = path.relative_to(game).as_posix()
        counts = Counter()
        with path.open('rb') as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
            if digest != expected[rel]['sha256'] or path.stat().st_size != expected[rel]['bytes']:
                raise ValueError(f'Archive differs from embedded-scan baseline: {rel}')
            with mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
                for _, crc, pos, length, compressed in catalog_tool.entries(path):
                    counts['entries'] += 1
                    identity = (rel, f'{crc:08x}', pos, length)
                    if identity in known:
                        found.add(identity)
                        counts['cataloged_top_level_entries_skipped'] += 1
                        continue
                    counts['uncataloged_entries'] += 1
                    if compressed:
                        counts['compressed_entries_skipped'] += 1
                        counts['compressed_bytes_skipped'] += compressed
                        continue
                    counts['entries_scanned'] += 1
                    counts['entry_bytes_scanned'] += length
                    signatures = scan_entry(data, pos, pos+length, sample_limit)
                    for name, result in signatures.items():
                        signature_counts[name] += result['count']
                        if name == 'dds':
                            dds_statuses.update(result['statuses'])
                        elif name == 'jpeg':
                            jpeg_statuses.update(result['statuses'])
                    if signatures:
                        counts['entries_with_signatures'] += 1
                        if len(hits) < entry_limit:
                            hits.append(dict(archive=rel, entry_crc=f'{crc:08x}', entry_offset=pos,
                                             entry_length=length, signatures=signatures))
        totals.update(counts)
        archives.append(dict(archive=rel, bytes=path.stat().st_size, sha256=digest, **counts))
    if found != known:
        raise ValueError(f'Missing cataloged top-level entries: {len(known-found)}')
    return dict(schema=1, complete=False, catalog_changes=0,
                method='Every-byte standard-signature search bounded to uncataloged uncompressed P5CK entries; no extraction.',
                catalog_sha256=hashlib.sha256(catalog_bytes).hexdigest(),
                baseline_sha256=hashlib.sha256(baseline_bytes).hexdigest(),
                totals=dict(totals), signature_counts=dict(signature_counts), dds_statuses=dict(dds_statuses),
                jpeg_statuses=dict(jpeg_statuses), jpeg_decoder_version=Image.__version__,
                report_limits=dict(samples_per_signature_per_entry=sample_limit, entries=entry_limit),
                entry_samples_truncated=totals['entries_with_signatures'] > len(hits),
                archives=archives, hits=hits,
                limitations=[
                    'Signature absence does not prove an entry texture-free or exclude it from overhaul scope.',
                    'PNG/XPR matches are signatures only; DDS checks bound the base level only.',
                    'JPEG candidates are entry-bounded Pillow decodes, capped at 16 megapixels; decoding does not establish texture use.',
                    'No mip-chain, executable-reference or runtime-identity proof is supplied.',
                    'Embedded texture containers remain in this scan; existing textures can produce incidental hits.',
                    'Compressed entries, other encodings, non-PAK files and procedural images are not decoded.',
                    'Sample limits affect report detail only; signature and decoder-status totals include every match.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, default=Path('private/game'))
    parser.add_argument('--root', type=Path, default=Path('private/texture-remaster'))
    args = parser.parse_args()
    report = scan(args.game, args.root)
    encoded = (json.dumps(report, indent=2)+'\n').encode('utf-8')
    if len(encoded) > 2*1024*1024:
        raise ValueError('Report exceeds 2 MiB; reduce sample limits before writing')
    (args.root/'archive-signature-scan.json').write_bytes(encoded)
    print(json.dumps({k: v for k, v in report.items() if k not in ('archives', 'hits')}, indent=2))


if __name__ == '__main__':
    main()
