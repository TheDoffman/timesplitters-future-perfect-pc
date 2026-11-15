"""Build the small trusted Darwin scanner-lifetime guardian and hash receipt."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'tools/mods/scan_guardian_mac.c'


def build(output):
    if platform.system() != 'Darwin':
        raise ValueError('Build the scanner guardian on the Mac host')
    output = Path(output).absolute()
    for member in (output, *output.parents):
        if member.is_symlink():
            raise ValueError('Linked guardian output refused')
    output.mkdir(parents=True, exist_ok=True)
    binary, receipt = output / 'scan-guardian-mac', output / 'receipt.json'
    source_sha = hashlib.sha256(SOURCE.read_bytes()).hexdigest()
    if binary.exists() or receipt.exists():
        if not binary.is_file() or binary.is_symlink() or not receipt.is_file() or receipt.is_symlink():
            raise ValueError('Preserving incomplete or unknown guardian output')
        if binary.stat().st_size > 1024**2 or receipt.stat().st_size > 4096:
            raise ValueError('Preserving oversized guardian output')
        old = json.loads(receipt.read_text())
        if old.get('producer') != 'tsfp-scan-guardian-v1' or hashlib.sha256(binary.read_bytes()).hexdigest() != old.get('sha256'):
            raise ValueError('Preserving unrecognized guardian output')
        if old.get('source_sha256') == source_sha:
            return old
    with tempfile.TemporaryDirectory(prefix='.scan-guardian-build-', dir=output) as temporary:
        staged = Path(temporary) / binary.name
        subprocess.run(['/usr/bin/clang', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                        str(SOURCE), '-o', str(staged)], check=True, timeout=60)
        subprocess.run(['/usr/bin/codesign', '--force', '--sign', '-', str(staged)], check=True, timeout=15)
        subprocess.run(['/usr/bin/codesign', '--verify', '--strict', str(staged)], check=True, timeout=15)
        record = dict(schema=1, producer='tsfp-scan-guardian-v1', source_sha256=source_sha,
                      sha256=hashlib.sha256(staged.read_bytes()).hexdigest(), architecture=platform.machine())
        metadata = Path(temporary) / receipt.name
        metadata.write_text(json.dumps(record, sort_keys=True) + '\n')
        metadata.chmod(0o600)
        os.replace(staged, binary)
        os.replace(metadata, receipt)
    return record


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/scan-guardian')
    args = parser.parse_args()
    print(json.dumps(build(args.output)))
