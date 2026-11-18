#!/usr/bin/env python3
"""Prepare a frozen runtime and manage one bound per-user backup schedule."""
import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import mod_backup_runtime as runtime
import mod_backup_schedule as schedule


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='operation', required=True)
    freeze = commands.add_parser('freeze')
    freeze.add_argument('source')
    freeze.add_argument('output')
    prepare = commands.add_parser('prepare')
    prepare.add_argument('output')
    prepare.add_argument('--runtime', required=True)
    prepare.add_argument('--runtime-sha256', required=True)
    prepare.add_argument('--control', required=True)
    prepare.add_argument('--binding-sha256', required=True)
    for operation in ('install', 'run', 'status', 'remove'):
        command = commands.add_parser(operation)
        command.add_argument('directory')
        command.add_argument('--schedule-sha256', required=True)
    args = parser.parse_args()
    try:
        if args.operation == 'freeze':
            value = runtime.prepare(args.source, args.output)
        elif args.operation == 'prepare':
            value = schedule.prepare(args.output, args.runtime, args.runtime_sha256, args.control, args.binding_sha256)
        else:
            value = schedule.manage(args.operation.title(), args.directory, args.schedule_sha256)
        print(json.dumps(value, indent=2))
        return 0
    except Exception:
        print('Backup runtime/schedule operation failed; preserve existing private state and tasks.', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
