"""Initialize or tick a private managed receiver; installs no scheduled task."""
import argparse
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import mod_backup_cipher as cipher
import mod_backup_receiver as receiver
from mod_package import ModError, read_json, require
from mod_trust import encode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('init', 'tick', 'status'))
    parser.add_argument('--pool', type=Path, required=True)
    parser.add_argument('--recipient-record', type=Path, required=True)
    parser.add_argument('--origin', required=True)
    parser.add_argument('--trusted-root-sha256', required=True)
    parser.add_argument('--ssh-host')
    parser.add_argument('--remote-python')
    parser.add_argument('--remote-entry')
    parser.add_argument('--remote-bundle')
    parser.add_argument('--remote-recipient-record')
    parser.add_argument('--keep', type=int, default=7)
    parser.add_argument('--interval', type=int, default=86400)
    parser.add_argument('--maximum-bytes', type=int, default=cipher.MAX_WIRE)
    parser.add_argument('--force', action='store_true')
    parser.add_argument('--seconds', type=int, default=receiver.transport.MAX_SECONDS)
    args = parser.parse_args()
    try:
        wire = receiver.read_private(cipher.private_path(args.recipient_record), 4096)
        recipient = read_json(wire)
        require(encode(recipient) == wire, 'Noncanonical recipient record')
        receiver.exports.recipients.checked_public(recipient, args.origin, args.trusted_root_sha256)
        remote = dict(host=args.ssh_host, python=args.remote_python, entry=args.remote_entry,
                      bundle=args.remote_bundle, record=args.remote_recipient_record)
        if args.operation == 'init':
            require(all(remote.values()) and not args.force and args.seconds == receiver.transport.MAX_SECONDS,
                    'Supply fixed SSH settings only when initializing')
            settings = receiver.initialize(args.pool, recipient, args.origin, args.trusted_root_sha256, remote,
                                           keep=args.keep, interval=args.interval, maximum_bytes=args.maximum_bytes)
            print(json.dumps(dict(schema=1, phase='backup-receiver-initialized', pool_id=settings['pool_id'])))
            return 0
        require(not any(remote.values()) and args.keep == 7 and args.interval == 86400
                and args.maximum_bytes == cipher.MAX_WIRE, 'Existing receiver policy is immutable')
        if args.operation == 'tick':
            result = receiver.run_ssh(args.pool, recipient, args.origin, args.trusted_root_sha256,
                                      force=args.force, seconds=args.seconds)
        else:
            require(not args.force and args.seconds == receiver.transport.MAX_SECONDS, 'Unexpected status options')
            with receiver.opened(args.pool, recipient, args.origin, args.trusted_root_sha256) as (pool, settings, db):
                rows = receiver.copies(db, settings, recipient, args.origin, args.trusted_root_sha256)
                receiver.totals(pool, rows, recipient, args.origin, args.trusted_root_sha256)
                result = receiver.report(db, settings, int(time.time()))
        print(json.dumps(result, indent=2))
        return 3 if result['stale_export'] else 0
    except (ModError, OSError, ValueError, sqlite3.Error, subprocess.SubprocessError, EOFError):
        print('Backup receiver failed; preserve partial copies and check private storage, trust and capacity.', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
