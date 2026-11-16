"""Export, verify or import off-server trust recovery using a local passphrase prompt."""
import argparse
import getpass
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from mod_package import ModError, require
from mod_portable_recovery import export_windows, import_windows, pin, read_export, unseal, write_export


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('export', 'check', 'import'))
    parser.add_argument('--store', type=Path)
    parser.add_argument('--file', required=True, type=Path)
    parser.add_argument('--trusted-root-sha256', required=True)
    args = parser.parse_args()
    try:
        pin(args.trusted_root_sha256)
        require((args.operation == 'check') == (args.store is None), 'Supply --store only for export or import')
        if args.store is not None:
            require(not args.file.absolute().is_relative_to(args.store.absolute()),
                    'Keep the portable export outside the recovery store')
        require(sys.stdin.isatty(), 'Run interactively; never put recovery passphrases in arguments, pipes or chat')
        password = getpass.getpass('Recovery passphrase (16-256 characters): ')
        if args.operation == 'export':
            require(password == getpass.getpass('Confirm recovery passphrase: '), 'Recovery passphrases differ')
            wire = export_windows(args.store, password, args.trusted_root_sha256)
            # Prove decryptability before committing the new encrypted file.
            _, result = unseal(wire, password, args.trusted_root_sha256)
            write_export(args.file, wire)
            result['phase'] = 'portable-recovery-exported'
        else:
            wire = read_export(args.file)
            if args.operation == 'import':
                result = import_windows(wire, password, args.trusted_root_sha256, args.store)
            else:
                _, result = unseal(wire, password, args.trusted_root_sha256)
        print(json.dumps(result, indent=2)); return 0
    except (OSError, ValueError, ModError, EOFError) as error:
        # Crypto/provider exceptions may include input snippets: print no exception data.
        print('Workshop recovery failed; check the passphrase, recorded pin, private paths and input integrity.', file=sys.stderr)
        return 2


if __name__ == '__main__': raise SystemExit(main())
