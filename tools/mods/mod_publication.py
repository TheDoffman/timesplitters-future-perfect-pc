"""Export approved releases to a separate, read-only static publication tree.

The public server never receives the repository database or quarantine path.
This is a download-only alpha; public upload/authentication is separate work.
"""
import json
from pathlib import Path
import shutil
import tempfile
import time

from mod_package import MAX_ARCHIVE, SHA, digest, read_json, require
from mod_repository import SCAN_TTL

MAX_CATALOG = 4 * 1024 * 1024
MAX_RELEASES = 1000
DEFAULT_PUBLIC_LIMIT = 512 * 1024 * 1024


def export_snapshot(repository, destination, byte_limit=DEFAULT_PUBLIC_LIMIT, signer=None):
    destination = Path(destination).absolute()
    require(not destination.exists() and not destination.is_symlink(),
            'Choose a new snapshot directory; existing publications are preserved')
    require(not destination.resolve().is_relative_to(repository.root),
            'Public snapshots must be outside the private repository')
    require(not repository.root.is_relative_to(destination.resolve()),
            'Public snapshots must not contain the private repository')
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.mod-publication-', dir=destination.parent) as temporary:
        staged = Path(temporary)
        (staged / 'v1/packages').mkdir(parents=True)
        with repository.db() as db:
            if signer is not None:
                db.execute('CREATE TABLE IF NOT EXISTS publication_sequence '
                           '(singleton INTEGER PRIMARY KEY CHECK(singleton=1), '
                           'version INTEGER NOT NULL, origin TEXT NOT NULL)')
            # Prevent concurrent moderation changes while constructing the snapshot.
            db.execute('BEGIN IMMEDIATE')
            releases = repository.catalog()
            require(len(releases) <= MAX_RELEASES, 'Too many releases for the alpha catalog')
            total = 0
            for release in releases:
                source = repository.approved_blob(release['sha256'])
                total += source.stat().st_size
                require(total <= byte_limit, 'Snapshot exceeds its public storage budget')
                target = staged / release['download'].lstrip('/')
                shutil.copyfile(source, target)
                require(digest(target.read_bytes()) == release['sha256'], 'Publication copy integrity failure')
            now = int(time.time())
            expiry = min([now + SCAN_TTL, *(r['scan']['time'] + SCAN_TTL for r in releases)])
            if signer is not None:
                from mod_trust import trusted_root
                root = trusted_root(signer.pinned_root, signer.updates, now)
                expiry = min(expiry, root['expires'])
            catalog = {'schema': 1, 'generated': now, 'expires': expiry, 'releases': releases}
            encoded = json.dumps(catalog, sort_keys=True, separators=(',', ':')).encode('utf-8')
            require(len(encoded) <= MAX_CATALOG, 'Public catalog exceeds the client limit')
            (staged / 'v1/catalog').write_bytes(encoded)
            if signer is not None:
                from mod_trust import MAX_REVOKED, MAX_VERSION
                old = db.execute('SELECT version,origin FROM publication_sequence WHERE singleton=1').fetchone()
                require(old is None or old['origin'] == root['origin'], 'Repository publication origin changed')
                version = 1 if old is None else old['version'] + 1
                require(version <= MAX_VERSION, 'Publication sequence exhausted')
                revoked = [r[0] for r in db.execute("SELECT sha256 FROM releases WHERE state='revoked' "
                                                    'ORDER BY sha256 LIMIT ?', (MAX_REVOKED + 1,))]
                require(len(revoked) <= MAX_REVOKED, 'Revocation metadata budget exhausted')
                for name, wire in signer.publish(encoded, version, revoked, now).items():
                    path = staged / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(wire)
                db.execute('INSERT INTO publication_sequence VALUES(1,?,?) '
                           'ON CONFLICT(singleton) DO UPDATE SET version=excluded.version',
                           (version, root['origin']))
                # Reserve before publication. A crash may leave a gap, never a
                # reused version for different metadata on this repository.
                db.commit()
            validate_snapshot(staged, trusted_root_wire=signer.pinned_root if signer is not None else None)
            staged.rename(destination)
    return {'snapshot': str(destination), 'releases': len(releases),
            'package_bytes': total, 'expires': expiry}


def validate_snapshot(root, trusted_root_wire=None, previous=None):
    """Preflight the exact publication root before starting the public file server."""
    root = Path(root).absolute()
    require(root.is_dir() and not root.is_symlink() and not root.is_junction(), 'Invalid snapshot root')
    # Reject links before reading metadata, including links at directory levels.
    actual = set()
    entries = 0
    for file in root.rglob('*'):
        entries += 1
        require(entries <= MAX_RELEASES + 140, 'Too many public snapshot entries')
        require(not file.is_symlink() and not file.is_junction(), 'Public links forbidden')
        if not file.is_dir():
            require(file.is_file(), 'Public special files forbidden')
            actual.add(file.relative_to(root).as_posix())
    path = root / 'v1/catalog'
    require(path.is_file() and path.stat().st_size <= MAX_CATALOG, 'Invalid public catalog')
    catalog = read_json(path.read_bytes())
    require(isinstance(catalog, dict) and catalog.get('schema') == 1, 'Invalid public schema')
    now = int(time.time())
    require(type(catalog.get('generated')) is int and type(catalog.get('expires')) is int
            and catalog['generated'] <= now < catalog['expires'] <= now + SCAN_TTL,
            'Public catalog has expired or invalid dates; rescan/review and export again')
    releases = catalog.get('releases')
    require(isinstance(releases, list) and len(releases) <= MAX_RELEASES, 'Invalid public releases')
    allowed = {'v1/catalog'}
    if trusted_root_wire is not None:
        from mod_trust import MAX_METADATA, MAX_ROOT, root_document, trusted_root, verify_catalog
        pinned_version = root_document(trusted_root_wire)['version']
        trust_path = root / 'v1/trust/catalog'
        require(trust_path.is_file() and trust_path.stat().st_size <= MAX_METADATA,
                'Signed catalog metadata is missing or too large')
        # Inspect at most64 successors, always in contiguous order. The pinned
        # root is supplied by the operator/release, never trusted from this tree.
        updates = []
        for version in range(pinned_version + 1, pinned_version + 65):
            update_path = root / ('v1/trust/roots/' + str(version))
            if not update_path.exists():
                break
            require(update_path.is_file() and update_path.stat().st_size <= MAX_ROOT,
                    'Invalid root rotation file')
            updates.append(update_path.read_bytes())
        authority = trusted_root(trusted_root_wire, updates, now)
        verify_catalog(trust_path.read_bytes(), path.read_bytes(), authority, previous, now)
        allowed.add('v1/trust/catalog')
        for version in range(pinned_version, authority['version'] + 1):
            allowed.add('v1/trust/roots/' + str(version))
        require((root / ('v1/trust/roots/' + str(pinned_version))).read_bytes() == trusted_root_wire,
                'Snapshot pinned root differs from the configured trust anchor')
    for release in releases:
        require(isinstance(release, dict), 'Invalid public release')
        sha = release.get('sha256')
        require(isinstance(sha, str) and SHA.fullmatch(sha), 'Invalid public release digest')
        name = 'v1/packages/' + sha
        require(release.get('download') == '/' + name and name not in allowed, 'Invalid public download')
        allowed.add(name)
    require(actual == allowed, 'Snapshot contains missing or unexpected files')
    total = 0
    for name in (n for n in allowed if n.startswith('v1/packages/')):
        package = root / name
        total += package.stat().st_size
        require(total <= DEFAULT_PUBLIC_LIMIT, 'Snapshot exceeds the public storage budget')
        require(package.stat().st_size <= MAX_ARCHIVE and digest(package.read_bytes()) == package.name,
                'Public package integrity failure')
    return catalog
