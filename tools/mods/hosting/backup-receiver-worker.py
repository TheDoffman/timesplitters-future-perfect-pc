#!/usr/bin/env python3
"""Operator entry for a bound receiver worker; does not register an OS task."""
import argparse
import json
import os
from pathlib import Path
import sys
import threading

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import mod_backup_worker as worker
from mod_package import read_json, require


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('init', 'tick', 'status'))
    parser.add_argument('control')
    parser.add_argument('--binding-sha256')
    parser.add_argument('--pool')
    parser.add_argument('--recipient-record')
    parser.add_argument('--origin')
    parser.add_argument('--trusted-root-sha256')
    args = parser.parse_args()
    timer = None
    try:
        if args.operation == 'init':
            require(args.pool and args.recipient_record and args.origin and args.trusted_root_sha256
                    and args.binding_sha256 is None, 'Supply the receiver authority when creating a worker')
            recipient = read_json(worker.receiver.read_private(Path(args.recipient_record), 4096))
            print(json.dumps(worker.initialize(args.control, args.pool, recipient, args.origin, args.trusted_root_sha256)))
            return 0
        require(args.binding_sha256 and not any((args.pool, args.recipient_record, args.origin, args.trusted_root_sha256)),
                'Existing worker authority must come from its pinned binding')
        if args.operation == 'status':
            value = worker.health(args.control, args.binding_sha256)
            print(json.dumps(value, indent=2))
            return 3 if value['missed_run'] or value['status'] != 'ok' else 0
        require(os.name == 'nt', 'Scheduled receiver process containment currently requires Windows')
        from mod_windows_job import attach
        attach()  # Before PowerShell, SSH or any other child.
        timer = threading.Timer(worker.receiver.transport.MAX_SECONDS + 120, lambda: os._exit(124))
        timer.daemon = True
        timer.start()
        return worker.tick(args.control, args.binding_sha256)
    except Exception:
        print('Backup worker failed; inspect its private health and receiver state.', file=sys.stderr)
        return 2
    finally:
        if timer is not None:
            timer.cancel()


if __name__ == '__main__':
    code = main()
    # An inherited/broken output pipe cannot block interpreter shutdown while
    # retaining the job handle and its children. There is no secret tick output.
    if len(sys.argv) > 1 and sys.argv[1] == 'tick':
        os._exit(code)
    raise SystemExit(code)
