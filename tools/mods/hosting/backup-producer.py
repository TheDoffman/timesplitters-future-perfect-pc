"""Initialize/tick an encrypted export cache; plaintext capture remains separate."""
import argparse
import json
from pathlib import Path
import sqlite3
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import mod_backup_cipher as cipher
import mod_backup_producer as producer
from mod_package import ModError, read_json, require
from mod_trust import encode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('init', 'tick', 'status'))
    parser.add_argument('--pool', type=Path, required=True)
    parser.add_argument('--recipient-record', type=Path, required=True)
    parser.add_argument('--origin', required=True)
    parser.add_argument('--trusted-root-sha256', required=True)
    parser.add_argument('--snapshot-pool', type=Path)
    parser.add_argument('--keep', type=int, default=1)
    parser.add_argument('--maximum-bytes', type=int, default=cipher.MAX_WIRE)
    args = parser.parse_args()
    try:
        wire = producer.journal.read_private(cipher.private_path(args.recipient_record), 4096)
        recipient = read_json(wire)
        require(encode(recipient) == wire, 'Noncanonical producer recipient record')
        if args.operation == 'init':
            require(args.snapshot_pool is not None, 'Select the fixed managed snapshot pool')
            settings = producer.initialize(args.snapshot_pool, args.pool, recipient, args.origin,
                                           args.trusted_root_sha256, keep=args.keep, maximum_bytes=args.maximum_bytes)
            print(json.dumps(dict(schema=1, phase='backup-producer-initialized', pool_id=settings['pool_id'])))
            return 0
        require(args.snapshot_pool is None and args.keep == 1 and args.maximum_bytes == cipher.MAX_WIRE,
                'Existing producer policy is immutable')
        if args.operation == 'tick':
            result = producer.run(args.pool, recipient, args.origin, args.trusted_root_sha256)
        else:
            with producer.opened(args.pool, recipient, args.origin, args.trusted_root_sha256) as (pool, settings, db, access):
                producer.journal.totals(pool, producer.journal.copies(db, settings, recipient, args.origin, args.trusted_root_sha256),
                                        recipient, args.origin, args.trusted_root_sha256)
                result = producer.report(pool, settings, db, int(time.time()))
        print(json.dumps(result, indent=2))
        return 4 if result['status'] == 'publication-pending' else (3 if result['stale_export'] else 0)
    except (ModError, OSError, ValueError, sqlite3.Error):
        print('Backup producer failed; preserve partial exports and check managed source, trust and capacity.', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
