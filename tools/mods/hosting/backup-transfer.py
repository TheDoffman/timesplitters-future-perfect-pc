"""Operator encrypted snapshot export/send/receive; no scheduler or host installation."""
import argparse
import json
from pathlib import Path
import sqlite3
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import mod_backup_cipher as cipher
import mod_backup_exports as exports
import mod_backup_transport as transport
from mod_package import ModError, read_json, require
from mod_trust import encode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('export', 'send', 'receive', 'verify'))
    parser.add_argument('--recipient-record', type=Path, required=True)
    parser.add_argument('--origin', required=True)
    parser.add_argument('--trusted-root-sha256', required=True)
    parser.add_argument('--snapshot', type=Path)
    parser.add_argument('--bundle', type=Path)
    parser.add_argument('--destination', type=Path)
    parser.add_argument('--ssh-host')
    parser.add_argument('--remote-python')
    parser.add_argument('--remote-entry')
    parser.add_argument('--remote-bundle')
    parser.add_argument('--remote-recipient-record')
    parser.add_argument('--seconds', type=int, default=transport.MAX_SECONDS)
    args = parser.parse_args()
    try:
        path = cipher.private_path(args.recipient_record)
        with cipher.incoming(path, 4096) as (stream, size, stable):
            wire = exports.bounded_read(stream, size)
            require(stream.read(1) == b'', 'Recipient record changed')
            stable()
        recipient = read_json(wire)
        require(encode(recipient) == wire, 'Noncanonical recipient record')
        exports.recipients.checked_public(recipient, args.origin, args.trusted_root_sha256)
        remote = (args.ssh_host, args.remote_python, args.remote_entry,
                  args.remote_bundle, args.remote_recipient_record)
        if args.operation == 'receive':
            require(all(remote) and args.destination is not None and args.snapshot is None
                    and args.bundle is None, 'Supply only the receive destination and fixed SSH settings')
            result = transport.receive_ssh(*remote, args.destination, recipient, args.origin,
                                           args.trusted_root_sha256, seconds=args.seconds)
        else:
            require(not any(remote) and args.destination is None and args.bundle is not None
                    and args.seconds == transport.MAX_SECONDS, 'Unexpected backup transfer arguments')
            if args.operation == 'export':
                require(args.snapshot is not None, 'Select a verified source snapshot')
                result = exports.export_snapshot(args.snapshot, args.bundle, recipient,
                                                  args.origin, args.trusted_root_sha256)
            else:
                require(args.snapshot is None, 'Snapshot source is only valid for export')
                if args.operation == 'send':
                    exports.send_stream(args.bundle, sys.stdout.buffer, recipient,
                                        args.origin, args.trusted_root_sha256)
                    return 0  # stdout is exclusively bounded binary transfer data.
                result = exports.inspect_export(args.bundle, recipient, args.origin,
                                                 args.trusted_root_sha256)[2]
        print(json.dumps(result, indent=2))
        return 0
    except (ModError, OSError, ValueError, sqlite3.Error, EOFError, TimeoutError, subprocess.SubprocessError):
        # Do not print underlying providers, SSH diagnostics, private paths or data.
        print('Backup transfer failed; preserve partial output and check trust, private paths and capacity.', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
