"""Explicit operator acceptance with real official ClamAV databases; no game data."""
import argparse
import json
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/mods'))
from mod_package import ModError, build_package
from mod_repository import ClamScanner, Repository


def package(root, identity, script):
    source = root / identity
    (source / 'scripts').mkdir(parents=True)
    manifest = dict(schema=1, id=identity, version='1.0.0', name='Scanner acceptance',
                    author='Local acceptance', description='Disposable scanner fixture',
                    game='tsfp-us-45410066', api='tsfp-mod-v1-draft',
                    dependencies={}, files={})
    (source / 'mod.json').write_text(json.dumps(manifest), encoding='utf-8')
    (source / 'scripts/main.lua').write_bytes(script)
    archive = root / (identity + '.zip')
    build_package(source, archive)
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--database', type=Path, required=True)
    parser.add_argument('--clamscan', default='clamscan')
    args = parser.parse_args()
    scanner = ClamScanner(args.clamscan, args.database)
    with tempfile.TemporaryDirectory(prefix='tsfp-live-clamav-') as temporary:
        root = Path(temporary)
        repo = Repository(root / 'quarantine')
        clean = repo.submit(package(root, 'scanner-clean', b'return {example = true}\n'), scanner)
        assert clean['state'] == 'pending-review', clean
        assert repo.catalog() == []
        repo.approve(clean['sha256'], 'Live acceptance: original fixture reviewed')
        assert len(repo.catalog()) == 1
        assert repo.approved_blob(clean['sha256']).is_file()

        # EICAR's harmless, standardized AV test string, never executed as code.
        eicar = (b'X5O!P%@AP[4\\PZX54(P^)7CC)7}$' + b'EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*')
        detected = repo.submit(package(root, 'scanner-eicar', eicar), scanner)
        assert detected['state'] == 'rejected', detected
        assert 'FOUND' in detected['scan']['detail'], detected
        try:
            repo.approve(detected['sha256'], 'Must be rejected')
        except ModError:
            pass
        else:
            raise AssertionError('Detected release was approved')
        assert len(repo.catalog()) == 1
        empty = root / 'empty-database'
        empty.mkdir()
        failed = repo.submit(package(root, 'scanner-error', b'return {}\n'),
                             ClamScanner(args.clamscan, empty))
        assert failed['state'] == 'scan-error', failed
        missing = ClamScanner(str(root / 'missing-scanner'), args.database).scan(root)
        assert missing['status'] == 'error', missing
        repo.revoke(clean['sha256'], 'Acceptance cleanup/revocation')
        assert repo.catalog() == []
        print(json.dumps(dict(clean=clean, detected=detected, empty_database=failed,
                              missing_scanner=missing['status'], revocation='passed'), indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
