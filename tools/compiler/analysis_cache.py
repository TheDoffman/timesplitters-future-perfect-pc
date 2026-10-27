"""Content receipts for project disassembly, independent of toolkit caches.

A receipt proves that inputs, options, implementation and output bytes all match.
Malformed or missing receipts are cache misses, never analysis success.
"""

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

from hashlib import file_digest, sha256
import json
from pathlib import Path, PurePosixPath
import tempfile
import time

from recomp_image import source_fingerprint


class AnalysisCache:
    VERSION = 2
    REQUIRED = ('summary.json', 'functions.json', 'xrefs.json', 'strings.json', 'labels.json')

    def __init__(self, output_dir):
        self.output_dir = Path(output_dir)
        self.cache_file = self.output_dir / '.disasm_cache.json'

    @staticmethod
    def _hash_file(filepath):
        with Path(filepath).open('rb') as stream:
            return file_digest(stream, 'sha256').hexdigest()

    @staticmethod
    def _opts_key(text_only, extra_sections=None, seed_functions=None, observed_seeds=None):
        options = dict(text_only=bool(text_only), extra_sections=sorted(set(extra_sections or ())),
                       seed_functions=sorted(set(seed_functions or ())),
                       observed_seeds=sorted(set(observed_seeds or ())),
                       source=source_fingerprint(),
                       cache_source=AnalysisCache._hash_file(__file__))
        return sha256(json.dumps(options, sort_keys=True, separators=(',', ':')).encode('utf-8')).hexdigest()

    def _load_cache(self):
        try:
            if self.cache_file.stat().st_size > 4 * 1024 * 1024:
                return None
            value = json.loads(self.cache_file.read_text(encoding='utf-8'))
            if not isinstance(value, dict) or value.get('version') != self.VERSION:
                return None
            return value
        except (OSError, UnicodeError, ValueError):
            return None

    def _artifact_path(self, relative):
        if not isinstance(relative, str) or '\\' in relative or ':' in relative:
            raise ValueError('Invalid receipt path')
        name = PurePosixPath(relative)
        if name.is_absolute() or not name.parts or '..' in name.parts:
            raise ValueError('Invalid receipt path')
        candidate = self.output_dir.joinpath(*name.parts)
        if not candidate.resolve().is_relative_to(self.output_dir.resolve()):
            raise ValueError('Receipt path escapes the output directory')
        return candidate

    def _inputs(self, xbe_path, analysis_json_path, text_only, extra_sections, seed_functions, observed_seeds):
        # summary.json names the executable, so identical bytes at a different
        # location still require republishing that provenance.
        return dict(xbe_path=str(Path(xbe_path).resolve()), xbe_hash=self._hash_file(xbe_path),
                    json_hash=self._hash_file(analysis_json_path) if analysis_json_path else None,
                    opts_key=self._opts_key(text_only, extra_sections, seed_functions, observed_seeds))

    def is_valid(self, xbe_path, analysis_json_path, text_only=False, extra_sections=None,
                 seed_functions=None, observed_seeds=None):
        receipt = self._load_cache()
        if receipt is None:
            return False
        try:
            current = self._inputs(xbe_path, analysis_json_path, text_only, extra_sections, seed_functions, observed_seeds)
            if any(receipt.get(key) != value for key, value in current.items()):
                return False
            artifacts = receipt.get('artifacts')
            if not isinstance(artifacts, dict) or not set(self.REQUIRED) <= artifacts.keys():
                return False
            return all(self._hash_file(self._artifact_path(name)) == digest for name, digest in artifacts.items())
        except (OSError, TypeError, ValueError):
            return False

    def save(self, xbe_path, analysis_json_path, text_only, elapsed_seconds,
             extra_sections=None, seed_functions=None, observed_seeds=None):
        inputs = self._inputs(xbe_path, analysis_json_path, text_only, extra_sections, seed_functions, observed_seeds)
        artifacts = {name: self._hash_file(self.output_dir / name) for name in self.REQUIRED}
        for path in sorted((self.output_dir / 'asm').glob('*.asm')):
            relative = path.relative_to(self.output_dir).as_posix()
            artifacts[relative] = self._hash_file(self._artifact_path(relative))
        receipt = dict(version=self.VERSION, timestamp=time.time(), elapsed_seconds=float(elapsed_seconds),
                       artifacts=artifacts, **inputs)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        # Publish one complete receipt only after every required output exists.
        pending = None
        try:
            with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8', dir=self.output_dir,
                                             prefix='.disasm-receipt-', suffix='.tmp', delete=False) as stream:
                pending = Path(stream.name)
                json.dump(receipt, stream, sort_keys=True, indent=2)
                stream.write('\n')
            pending.replace(self.cache_file)
        finally:
            if pending is not None:
                pending.unlink(missing_ok=True)

    def get_last_run_time(self):
        receipt = self._load_cache()
        value = receipt.get('elapsed_seconds') if receipt is not None else None
        return value if isinstance(value, (int, float)) and not isinstance(value, bool) else None

    def invalidate(self):
        self.cache_file.unlink(missing_ok=True)
