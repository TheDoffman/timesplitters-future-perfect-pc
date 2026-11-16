"""Locked-account configuration step; requires root coordinator quiescence admission."""
import argparse
import json
import os
from pathlib import Path
import stat
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from mod_package import ModError, require
import mod_host_migration as migration


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group()
    action.add_argument('--inspect', action='store_true')
    action.add_argument('--apply', action='store_true')
    action.add_argument('--rollback', action='store_true')
    parser.add_argument('--from-release')
    parser.add_argument('--nonce')
    args = parser.parse_args()
    try:
        if not (args.inspect or args.apply or args.rollback):
            require(args.from_release is None and args.nonce is None, 'Choose an explicit migration action')
            value = dict(schema=1, phase='plan-only', files=6, services_changed=False, data_written=False)
        else:
            require(isinstance(args.from_release, str) and migration.SHA.fullmatch(args.from_release), 'An exact base release is required')
            host, scan = migration.helpers()
            after = scan.installed_release()  # Enforces Darwin, locked IDs/groups and installed runtime.
            script = Path(__file__).absolute();info = script.lstat()
            require(script == after/'source/tools/mods/hosting/migrate-private-generation-mac.py'
                    and stat.S_ISREG(info.st_mode) and info.st_uid == 0 and info.st_nlink == 1
                    and not info.st_mode & 0o022, 'Use the immutable installed migration step')
            before = migration.ROOT/'releases'/args.from_release
            if args.inspect:
                require(args.nonce is None, 'Inspection takes no migration nonce')
                value = migration.inspect(before, after)
            else:
                value = migration.migrate(before, after, args.nonce, rollback=args.rollback)
        print(json.dumps(value, indent=2));return 0
    except (OSError, ValueError, ModError, subprocess.SubprocessError) as error:
        print('Private generation migration: '+str(error)[:1000], file=sys.stderr);return 2


if __name__ == '__main__':raise SystemExit(main())
