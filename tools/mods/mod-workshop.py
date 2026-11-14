"""Developer/operator CLI for the mod platform prototype (no game activation)."""
import argparse
import json
import sqlite3
import sys

from mod_package import ModError, build_package, inspect_package
from mod_repository import ClamScanner, Repository, stage_install
from mod_server import fetch_install, make_server
from mod_publication import export_snapshot, validate_snapshot
from mod_accounts import Accounts, ServiceError
from mod_uploads import Uploads
from mod_operator import issue_invitation


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    build = commands.add_parser('pack', help='Build a creator package')
    build.add_argument('source')
    build.add_argument('output')
    validate = commands.add_parser('validate')
    validate.add_argument('package')
    install = commands.add_parser('install', help='Verify and stage a downloaded package inactive')
    install.add_argument('package')
    install.add_argument('--sha256', required=True)
    install.add_argument('--mods', required=True)
    fetch = commands.add_parser('fetch-install', help='Download from the local preview and stage inactive')
    fetch.add_argument('--server', required=True)
    fetch.add_argument('--sha256', required=True)
    fetch.add_argument('--mods', required=True)
    export = commands.add_parser('export', help='Export fresh reviewed releases for static Mac hosting')
    export.add_argument('--repository', required=True)
    export.add_argument('--output', required=True)
    export.add_argument('--trusted-root', help='Out-of-band bootstrap root file')
    export.add_argument('--root-update', action='append', default=[])
    export.add_argument('--signing-key', action='append', default=[])
    export.add_argument('--unsigned-preview', action='store_true', help='Local development only; public host refuses it')
    check_public = commands.add_parser('check-public', help='Preflight a static public snapshot')
    check_public.add_argument('snapshot')
    check_public.add_argument('--trusted-root', required=True)
    for name in ('invite','recover-account','suspend-account','assign-owner','scan-next'):
        command = commands.add_parser(name)
        command.add_argument('--repository', required=True)
        if name != 'scan-next':
            command.add_argument('username')
        if name == 'invite':
            command.add_argument('--role', choices=('creator','moderator','admin'), default='creator')
        if name in ('recover-account','suspend-account','assign-owner'):
            command.add_argument('--note', required=True)
        if name == 'assign-owner':
            command.add_argument('--mod-id', required=True)
        if name == 'scan-next':
            command.add_argument('--clamscan', default='clamscan')
            command.add_argument('--clamav-database', required=True)
    for name in ('submit', 'approve', 'revoke', 'rescan', 'search', 'serve', 'inspect'):
        command = commands.add_parser(name)
        command.add_argument('--repository', required=True)
        if name == 'submit':
            command.add_argument('package')
        if name in ('submit', 'rescan'):
            command.add_argument('--clamscan', default='clamscan', help='Trusted scanner executable')
            command.add_argument('--clamav-database', help='Operator-owned official ClamAV database directory')
        if name in ('approve', 'revoke', 'rescan', 'inspect'):
            command.add_argument('sha256')
        if name in ('approve', 'revoke'):
            command.add_argument('--note', required=True)
        if name == 'search':
            command.add_argument('query', nargs='?', default='')
        if name == 'serve':
            command.add_argument('--port', type=int, default=8765)
    args = parser.parse_args(argv)
    try:
        if args.command == 'pack':
            result = build_package(args.source, args.output)
        elif args.command == 'validate':
            result = inspect_package(args.package)
        elif args.command == 'install':
            result = stage_install(args.package, args.sha256, args.mods)
        elif args.command == 'fetch-install':
            result = fetch_install(args.server, args.sha256, args.mods)
        elif args.command == 'export':
            from mod_package import require
            require(args.unsigned_preview != bool(args.trusted_root),
                    'Supply --trusted-root with catalog keys, or explicitly choose --unsigned-preview')
            signer = None
            if args.trusted_root:
                from mod_signing import load_signer
                signer = load_signer(args.trusted_root, args.signing_key, args.root_update)
            else:
                require(not args.signing_key and not args.root_update, 'Unsigned preview cannot use signing inputs')
            result = export_snapshot(Repository(args.repository), args.output, signer=signer)
        elif args.command == 'check-public':
            from mod_signing import bounded_file
            from mod_trust import MAX_ROOT
            result = validate_snapshot(args.snapshot, bounded_file(args.trusted_root, MAX_ROOT))
        elif args.command in ('invite','recover-account','suspend-account','assign-owner','scan-next'):
            accounts = Accounts(Repository(args.repository))
            if args.command == 'invite':
                result = issue_invitation(accounts, args.username, args.role)
            elif args.command == 'recover-account':
                result = issue_invitation(accounts, args.username, recovery_note=args.note)
            elif args.command == 'suspend-account':
                accounts.suspend(args.username, args.note)
                result = {'username': args.username, 'state': 'suspended'}
            elif args.command == 'assign-owner':
                Uploads(accounts).assign_owner(args.mod_id, args.username, args.note)
                result = {'id': args.mod_id, 'username': args.username}
            else:
                result = Uploads(accounts).process_next(ClamScanner(args.clamscan, args.clamav_database))
        else:
            repo = Repository(args.repository)
            if args.command == 'submit':
                result = repo.submit(args.package, ClamScanner(args.clamscan, args.clamav_database))
            elif args.command == 'rescan':
                result = repo.rescan(args.sha256, ClamScanner(args.clamscan, args.clamav_database))
            elif args.command == 'approve':
                repo.approve(args.sha256, args.note)
                result = {'state': 'approved', 'sha256': args.sha256}
            elif args.command == 'revoke':
                repo.revoke(args.sha256, args.note)
                result = {'state': 'revoked', 'sha256': args.sha256}
            elif args.command == 'inspect':
                result = repo.get(args.sha256)
            elif args.command == 'search':
                result = repo.catalog(args.query)
            elif args.command == 'serve':
                server = make_server(repo, args.port)
                print(f'Local preview: http://127.0.0.1:{server.server_port}', flush=True)
                try:
                    server.serve_forever()
                finally:
                    server.server_close()
                return 0
        print(json.dumps(result, indent=2))
        if args.command in ('submit', 'rescan') and result['state'] in ('scan-error', 'rejected'):
            return 2
        return 0
    except (ModError, ServiceError, OSError, sqlite3.Error) as exc:
        print('Mod workshop: ' + str(exc), file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        return 130


if __name__ == '__main__':
    raise SystemExit(main())
