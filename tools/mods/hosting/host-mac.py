"""Run only a preflighted static publication, stopping at its trust expiry."""
import argparse
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from mod_package import ModError, require
from mod_publication import validate_snapshot


def serve(snapshot, trusted_root_wire, caddy='caddy'):
    require(platform.system() == 'Darwin', 'Run this host command on the Mac mini')
    root = Path(snapshot).absolute()
    require(not any(c in str(root) for c in '"{}\r\n'), 'Invalid snapshot path for the host configuration')
    catalog = validate_snapshot(root, trusted_root_wire=trusted_root_wire)
    executable = shutil.which(caddy)
    require(executable is not None, 'Install Caddy before starting the workshop host')
    config = Path(__file__).with_name('Caddyfile')
    env = dict(os.environ, TSFP_MOD_PUBLIC_ROOT=str(root))
    subprocess.run([executable, 'validate', '--config', str(config), '--adapter', 'caddyfile'],
                   env=env, check=True, timeout=15)
    seconds = catalog['expires'] - time.time()
    require(seconds > 0, 'Snapshot expired during preflight; export a fresh snapshot')
    print('Serving approved downloads at 127.0.0.1:8766 until snapshot expiry. '
          'Public access requires a separate HTTPS connection.', flush=True)
    try:
        result = subprocess.run([executable, 'run', '--config', str(config), '--adapter', 'caddyfile'],
                                env=env, timeout=seconds)
        require(result.returncode == 0, 'The static file server stopped with an error')
    except subprocess.TimeoutExpired:
        # subprocess.run kills and reaps its child on timeout. Never serve a
        # stale scan snapshot indefinitely after the operator leaves the Mac.
        raise ModError('Publication expired; static hosting stopped. Rescan/review and export again.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('snapshot')
    parser.add_argument('--trusted-root', required=True, help='Operator-controlled bootstrap trust anchor')
    args = parser.parse_args()
    try:
        from mod_signing import bounded_file
        from mod_trust import MAX_ROOT
        serve(args.snapshot, bounded_file(args.trusted_root, MAX_ROOT))
        return 0
    except (ModError, OSError, subprocess.SubprocessError) as exc:
        print('Workshop host: ' + str(exc), file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        return 130


if __name__ == '__main__':
    raise SystemExit(main())
