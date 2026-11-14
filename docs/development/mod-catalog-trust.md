# Workshop publication trust

The producer and shared native launcher implement this protocol. It is not a
released public service. `mod_trust.py` and `ModTrustPage.cs` verify real signatures;
the launcher requires signed catalogs and saves their verified root chain and
catalog checkpoint. Release-pinned production roots, public deployment and
internet acceptance are still required. DefaultRootBase64/DefaultService remain
blank until the real offline ceremony and endpoint verification.

## Wire contract

`/v1/catalog` retains the bounded schema1 catalog. Its exact UTF8 bytes are bound
by `/v1/trust/catalog`, an envelope containing exactly `payload` (canonical
standard padded base64) and `signatures` (one to eight entries). Each entry has
exactly `keyid` and `signature`. Duplicate JSON fields and duplicate signer IDs
are rejected. Unknown fields are rejected in trust documents. The envelope limit
is2MiB; root envelopes are64KiB. The decoded payload is at most half that limit.
Shared JSON budgets still apply: depth32,4096 members per collection and65536
tokens. RSA signatures have384 bytes. Signature input is the ASCII/UTF8 bytes
`TSFP-WORKSHOP-METADATA-V1`, one zero byte, then the **decoded payload bytes**.
Do not parse/reserialize a payload before verifying its signature.

The fixed algorithm is RSA-3072, exponent65537, PKCS#1v1.5 signatures with
SHA256, named `rsa-pkcs1-sha256`. This uses the PyCA/OpenSSL implementation,
not handwritten RSA. These primitives are available in the existing Windows
.NET Framework and native Mac/Linux .NET runtimes. There is no algorithm
negotiation, arbitrary exponent, smaller key, or signature/encryption reuse.
PyCA documents [RSA signing and verification](https://cryptography.io/en/stable/hazmat/primitives/asymmetric/rsa/).
Operator dependencies are pinned in `requirements.txt`; host/CI wheel hashes
are in `tools/mods/hosting/requirements-signing.txt` (official PyPI version JSON).

Public keys contain exactly `scheme`, `modulus` (384-byte big-endian integer,
standard base64), and `exponent` (`AQAB`). Their key ID is SHA256 of compact,
ASCII JSON with ordinally sorted keys, with no whitespace. Thus the key-ID
input has the field order `exponent`, `modulus`, `scheme`. The modulus must
have3072 bits and be odd. A different key descriptor cannot reuse an old ID.

Roots have exactly `type` (`tsfp-workshop-root`), `schema` (integer1), `version`,
`origin`, `issued`, `expires`, `keys` and `roles`. Origins are canonical lowercase
HTTPS DNS origins, without port, trailing slash, credentials, query or fragment.
The lifetime is at most366 days. Integers are actual JSON integers (not bools,
fractions or strings), at most2^53-1. The root role needs at least two signatures
from its two to four keys. Catalog roles need one or more signatures. Role keys
are disjoint; there are three to eight keys total and no unused keys.

Initial trust comes from a release-pinned/out-of-band root. A root downloaded
from the workshop is **never** a new trust anchor. Root successors are exposed
at `/v1/trust/roots/<version>`, with contiguous versions and unchanged origin.
Each successor needs both the previous root threshold and its own root threshold.
Expired old roots can authorize recovery to a live successor; the final root
must be live and not issued in the future. Validation accepts at most64 sequential
rotations per chain. The operator must distribute a release containing a newer
verified anchor before that bound is reached. Old catalog keys lose authority
when the successor changes the catalog role.

The catalog trust payload has exactly `type` (`tsfp-workshop-catalog`), integer
`schema`1, `origin`, `root_version`, `version`, `generated`, `expires`,
`catalog_sha256`, `catalog_length`, and sorted unique `revoked` package hashes.
It must use the current root version and origin. The exact catalog length and
digest must match. Catalog and trust metadata dates must agree, the lifetime is
at most24h, and expiry cannot exceed the root expiry or oldest included scan.
Revoked packages cannot be advertised. Revocation metadata currently caps at
4000 hashes; exceeding that bound fails publication rather than silently dropping
revocations. A reviewed sharding/protocol extension is required before the limit.

`verify_catalog` takes the caller's previous trusted checkpoint and returns an
updated one. It rejects lower root/catalog versions, same catalog version with
different signed metadata, clock rollback and forgotten revocations. The same
immutable unexpired publication may be reread. A client must atomically persist
the checkpoint and accepted root chain before offering installs, and must never
silently reset damaged state. The native implementation does this as described
below. Local device
administrators who can replace the launcher/pins/state remain outside this threat
boundary. Blocking updates can withhold revocations only until the24h expiry;
there is no claim of instantaneous revocation from an offline static snapshot.

## Shared native enforcement

`ModTrustPage.cs` contains the verifier/store shared by WPF and Avalonia. RSA uses
the platform .NET implementation with fixed RSA3072/PKCS1/SHA256 parameters. The
Page suffix includes this model in the existing shared-source build pattern;
there is no separate trust policy in OS adapters. Before release, set the compiled
`ModTrustStore.DefaultRootBase64` from the independently verified production root.
While that pin is blank, developers/operators may explicitly install a public
root at `Support/Mods/trusted-root.json`. It is never fetched as an initial pin.
A compiled pin takes precedence over that file. Changing a configured pin cannot
silently reset an existing checkpoint. No private key goes into a launcher.

`ModWorkshopClient` is bound to an installation folder. Read first validates its
saved signed state, fetches bounded metadata and required sequential root updates,
verifies exact catalog bytes/expiry/origin, checks the last checkpoint, validates
release details, and commits before returning listings. Trust folders are keyed
by the canonical HTTPS origin under `.launcher/mod-trust/<origin-sha256>`.
`state.json` contains exactly schema1, the pinned-root hash, base64 root successors,
base64 signed metadata, base64 hash-bound catalog bytes, and observation time.
The complete state is at most16MiB. All signatures and byte hashes are checked
again on reopening, including after restart; expired old state may authorize a
refresh, but cannot authorize an install/activation/game mount.

A per-origin exclusive file lock covers read/update. A unique exclusive partial
file is flushed before atomic replacement. The `initialized` marker is flushed
before the first commit; missing state after initialization fails closed. Damage,
changed pins, rollback, concurrent writers and clock rollback never fall back to
empty state. Failed/cancelled transfers keep the previous checkpoint and remove
only their own partial file. No power-loss recovery guarantee is inferred from
atomic file replacement; crash/backup recovery remains an operational gate.

The native client requires HTTPS on its default port and refuses redirects,
userinfo, external package URLs, URL paths/queries/fragments and unsigned catalogs.
The standalone Python loopback preview remains a local developer tool; it is not
an unsigned bypass in the native workshop. The client rechecks availability before
and after package transfer, then keeps only exact SHA256 bytes in its bounded
download cache. Revocation during transfer rejects/removes that partial package.

Native workshop installs write schema2 receipts with their verified service
origin. Local creator/legacy receipts retain schema1 without public provenance.
Workshop install, enable and normal game launch require a fresh locally verified
catalog, a still-advertised package and no revocation. The checks batch packages
per origin. An expired or revoked workshop package can be disabled/removed; the
original-game recovery path remains available through Play without mods. Expiry
does not silently rewrite the profile, delete packages, or affect an unmodded game.

## Key ceremony and export

Run production secret management as a non-root Unix operator. Private keys need
an operator-owned0700 directory and0600 regular file. Key/directory links and
directory aliases are refused. New output paths are exclusive: interrupted or
existing secret directories are preserved, never overwritten. No passphrase or
private key appears in process arguments, printed output or public snapshots.
Use a host-local environment with pinned signing dependencies:

```sh
python3 -m pip install --require-hashes -r tools/mods/hosting/requirements-signing.txt
```

On an offline machine, generate at least two independent encrypted root keys.
The tool prompts without echo for a passphrase of at least20 characters. Store
the keys/backups separately; don't copy root private keys to the public host:

```sh
python3 tools/mods/mod_signing.py keygen /absolute/offline-root-a --offline
python3 tools/mods/mod_signing.py keygen /absolute/offline-root-b --offline
```

On the host, generate its dedicated catalog key. This key is unencrypted so the
publisher can eventually run unattended, and depends on private filesystem/service
identity isolation. It cannot change the root role. Transfer only its public JSON
to the offline ceremony machine. After obtaining and verifying the actual HTTPS
hostname (the example below is a placeholder), sign the bootstrap root:

```sh
python3 tools/mods/mod_signing.py keygen /absolute/online-catalog-key
python3 tools/mods/mod_signing.py root \
  --root-key /absolute/offline-root-a/private.pem \
  --root-key /absolute/offline-root-b/private.pem \
  --catalog-public /absolute/transferred-catalog-public.json \
  --origin https://verified-host.example.test --output /absolute/root-1.json
```

For a rotation, supply the new encrypted root keys/catalog public descriptor,
`--previous-root /absolute/root-N.json`, and enough `--old-root-key` inputs to
meet the old threshold. The output is cross-signed versionN+1. If retaining the
same root keys, their signatures satisfy both roles. Test the resulting chain
before using the new catalog key. Supply successors in order with repeated
`--root-update` arguments during export.

Copy only public roots and the online catalog key to the publisher. Independently
verify/distribute the initial root; fetching it over the same workshop connection
does not establish trust. Keep the bootstrap root's identity stable in the client.

```sh
python3 tools/mods/mod-workshop.py export \
  --repository /absolute/private-repository --output /absolute/new-public-snapshot \
  --trusted-root /absolute/root-1.json --signing-key /absolute/online-catalog-key/private.pem
python3 tools/mods/mod-workshop.py check-public /absolute/new-public-snapshot \
  --trusted-root /absolute/root-1.json
sh tools/mods/hosting/run-mac-mod-host.sh /absolute/new-public-snapshot /absolute/root-1.json
```

The repository stores a monotonic `publication_sequence` in its private SQLite
database. The sequence is committed before a snapshot is exposed; failure can
leave a gap, but this database never reuses a reserved version. Changed origins
are refused. Keep this database and all revocation history in backups. Restoring
an older database needs a separately verified high-water mark greater than any
published version; do not publish directly from a stale restore. Full automated
backup/high-water recovery is pending and remains a public deployment gate.

The host preflights an independently supplied anchor, the signed catalog, root
chain, exact file set, packages and expiry before starting Caddy. It stops at
expiry. Private database/quarantine/keys are outside the publication root.
`export --unsigned-preview` remains for development. The public host and
`check-public` refuse unsigned trees; the local loopback preview is not a trusted
public endpoint. Caddy serves only GET/HEAD for the plain catalog, trust metadata,
numbered public roots and approved digest-named packages.

## Evidence and remaining work

`tests/mods/test_catalog_trust.py` uses real disposable RSA keys, not signature
mocks, for tampering, expiry, rollback, equivocation, two-threshold rotation,
cross-origin rejection, forgotten revocations, counter persistence and failed
publication reservation. Unix checks cover private file permissions and an actual
encrypted-root/public-descriptor-to-online-export ceremony. No fixture key is a
production key. The existing host expiry test now uses a signed snapshot.

Still required: native host/public acceptance, compiled production trust pins,
live publication switching, a real
offline production ceremony, separate service identities/backups, and public
internet acceptance. Antivirus scanning and signature validation do not prove
that gameplay scripts are safe; their sandbox/API remains a separate requirement.

`tests/launcher/workshop-trust-fixtures.json` contains original synthetic DDS/ZIP
bytes and public envelopes generated by Python/PyCA with disposable keys. No
private fixture key was retained. Shared `workshop_trust.cs` checks real native
verification of those producer signatures, cross-signed rotation, saved-state
restart/rollback/corruption, exact bytes, expiry, revocations before activation
and mounting, concurrent writers, cancelled/oversized metadata, and revocation
during transfer. `test_native_trust_fixtures.py` independently verifies those same
public vectors with the producer and validates the original asset package. These
checks use an injected test clock, not a production clock/verification bypass.
